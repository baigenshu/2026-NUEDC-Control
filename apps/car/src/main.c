/**
 * @file main.c
 * @brief car 底盘主循环：B21 演示开关 + 邮箱仲裁 + Chassis_Update
 *
 * 优先级：急停 > 遥控 > Busy(MOTION) > 巡线 > 仅 IDLE 时 Stop
 * 控制节拍：SysTick 软计时，约 10 ms（禁止用 TIMG0）
 *
 * 板载 B21（PB21）：按下=低。上电后等待一次有效单击再跑演示；
 * 演示中再按可中止。演示依次覆盖：差速 / 持续与定距直行 / 持续与定角转向 /
 * 停车 / 巡线 / Abort（见 run_boot_demo）。
 */
#include "ti_msp_dl_config.h"
#include "chassis.h"
#include "chassis_cfg.h"
#include "line_track.h"

/* ---------- 简易 SysTick 毫秒时基 ---------- */
#ifndef SYSTICK_HZ
#define SYSTICK_HZ  80000000u
#endif

#ifndef KEY_DEBOUNCE_MS
#define KEY_DEBOUNCE_MS  20u
#endif

/* 1=上电等 B21 后跑功能演示；0=直接进主循环 */
#ifndef CAR_BOOT_DEMO
#define CAR_BOOT_DEMO  1
#endif

/* 演示参数（时长/距离尽量短，便于桌面验证） */
#ifndef DEMO_HOLD_MS
#define DEMO_HOLD_MS           800u
#endif
#ifndef DEMO_PAUSE_MS
#define DEMO_PAUSE_MS          400u
#endif
#ifndef DEMO_LINE_MS
#define DEMO_LINE_MS           3000u
#endif
#ifndef DEMO_GO_CM
#define DEMO_GO_CM             30.f
#endif
#ifndef DEMO_TURN_DEG
#define DEMO_TURN_DEG          90.f
#endif

static uint32_t s_ms;
static uint32_t s_last_systick;
static uint32_t s_cycle_accum;

static void timebase_init(void)
{
    s_ms           = 0;
    s_last_systick = SysTick->VAL;
    s_cycle_accum  = 0;
}

static uint32_t millis(void)
{
    uint32_t now = SysTick->VAL;
    uint32_t elapsed;

    if (s_last_systick >= now)
        elapsed = s_last_systick - now;
    else
        elapsed = s_last_systick + (SysTick->LOAD + 1u) - now;

    s_last_systick = now;
    s_cycle_accum += elapsed;

    while (s_cycle_accum >= (SYSTICK_HZ / 1000u)) {
        s_cycle_accum -= (SYSTICK_HZ / 1000u);
        s_ms++;
    }
    return s_ms;
}

/* ---------- B21 按键（上拉，按下=低）---------- */
static bool key_is_down(void)
{
    return DL_GPIO_readPins(GPIO_KEY_PORT, GPIO_KEY_B21_PIN) == 0u;
}

/** 阻塞等待一次单击（按下并松开，带消抖） */
static void key_wait_click(void)
{
    uint32_t t0;

    /* 若上电时已按住，先等松开 */
    while (key_is_down())
        (void)millis();

    /* 等按下 */
    for (;;) {
        while (!key_is_down())
            (void)millis();
        t0 = millis();
        while ((millis() - t0) < KEY_DEBOUNCE_MS)
            ;
        if (key_is_down())
            break;
    }

    /* 等松开 */
    while (key_is_down())
        (void)millis();
    t0 = millis();
    while ((millis() - t0) < KEY_DEBOUNCE_MS)
        ;
}

/**
 * 非阻塞：检测一次新的按下边沿（消抖）。
 * 返回 true 表示本拍确认按下（调用方应处理一次事件）。
 */
static bool key_poll_press(void)
{
    static uint8_t  stable;   /* 0=松开 1=按下 */
    static uint8_t  raw_last;
    static uint32_t edge_ms;
    uint8_t raw = key_is_down() ? 1u : 0u;
    uint32_t now = millis();

    if (raw != raw_last) {
        raw_last = raw;
        edge_ms  = now;
        return false;
    }
    if ((now - edge_ms) < KEY_DEBOUNCE_MS)
        return false;
    if (raw != stable) {
        stable = raw;
        if (stable)
            return true; /* 按下边沿 */
    }
    return false;
}

/* ---------- 命令邮箱（ISR 只写，主循环只读）---------- */
typedef struct {
    volatile uint8_t estop;
    volatile uint8_t remote_active;
    volatile int16_t throttle;
    volatile int16_t turn;
    volatile uint8_t line_enable;
} cmd_mailbox_t;

static cmd_mailbox_t s_mbox;

void CmdMailbox_SetEstop(uint8_t on)
{
    s_mbox.estop = on ? 1u : 0u;
}

void CmdMailbox_SetRemote(int16_t throttle, int16_t turn)
{
    s_mbox.throttle      = throttle;
    s_mbox.turn          = turn;
    s_mbox.remote_active = 1u;
}

void CmdMailbox_ClearRemote(void)
{
    s_mbox.remote_active = 0u;
}

void CmdMailbox_SetLineEnable(uint8_t on)
{
    s_mbox.line_enable = on ? 1u : 0u;
}

#if CAR_BOOT_DEMO
/**
 * 演示节拍：推进 Chassis_Update，B21 按下则 Abort 并返回 false。
 * @return true=正常跑满 duration_ms；false=用户中止
 */
static bool demo_run_ms(uint32_t duration_ms)
{
    uint32_t t_prev = millis();
    uint32_t t0    = t_prev;

    while ((millis() - t0) < duration_ms) {
        uint32_t now = millis();
        uint32_t dt  = now - t_prev;
        if (dt < 5u)
            continue;
        if (dt > 50u)
            dt = 50u;
        t_prev = now;
        Chassis_Update(dt);
        if (key_poll_press()) {
            LineTrack_SetEnable(false);
            Chassis_Abort();
            return false;
        }
    }
    return true;
}

/** 等 MOTION 结束（Busy→false）；B21 可中止 */
static bool demo_wait_motion(void)
{
    uint32_t t_prev = millis();

    while (Chassis_Busy()) {
        uint32_t now = millis();
        uint32_t dt  = now - t_prev;
        if (dt < 5u)
            continue;
        if (dt > 50u)
            dt = 50u;
        t_prev = now;
        Chassis_Update(dt);
        if (key_poll_press()) {
            LineTrack_SetEnable(false);
            Chassis_Abort();
            return false;
        }
    }
    return true;
}

static bool demo_pause(void)
{
    Chassis_Stop(CHASSIS_STOP_DEFAULT);
    return demo_run_ms(DEMO_PAUSE_MS);
}

/**
 * 功能联调演示（顺序，B21 任意时刻可中止）：
 *  1. SetLR 差速
 *  2. Arcade 油门/转向
 *  3. 持续直行 Go(NULL) + 停车
 *  4. 定距直行 Go(distance) + straighten
 *  5. 定距后退
 *  6. 持续自旋 Turn(angle=0)
 *  7. 定角左转 / 定角右转
 *  8. 巡线短时使能（无黑线则按丢线策略安静停）
 *  9. ResetOdom / 短直行验证里程
 * 10. Abort 收尾
 */
static void run_boot_demo(void)
{
    LineTrack_SetEnable(false);
    Chassis_SetSurface(CHASSIS_SURFACE_NORMAL);
    Chassis_SetSpeedMode(CHASSIS_MODE_OPENLOOP);
    Chassis_ResetOdom();
    Chassis_Enable(true);

    /* 1. 即时差速 SetLR：左慢右快 → 微右弧 */
    Chassis_SetLR(CHASSIS_SPEED_SLOW, CHASSIS_SPEED_DEFAULT);
    if (!demo_run_ms(DEMO_HOLD_MS))
        return;
    if (!demo_pause())
        return;

    /* 2. Arcade：前进 + 微左转 */
    Chassis_Arcade(CHASSIS_SPEED_DEFAULT, 20);
    if (!demo_run_ms(DEMO_HOLD_MS))
        return;
    if (!demo_pause())
        return;

    /* 3. 持续直行 HOLD，再 Stop */
    Chassis_Go(CHASSIS_SPEED_DEFAULT, NULL);
    if (!demo_run_ms(DEMO_HOLD_MS))
        return;
    Chassis_Stop(CHASSIS_STOP_BRAKE);
    if (!demo_run_ms(DEMO_PAUSE_MS))
        return;

    /* 4. 定距前行 + 航向纠偏 */
    Chassis_Go(CHASSIS_SPEED_DEFAULT,
               &(chassis_go_opt_t){ .distance_cm = DEMO_GO_CM, .straighten = true });
    if (!demo_wait_motion())
        return;
    if (!demo_pause())
        return;

    /* 5. 定距后退 */
    Chassis_Go((int16_t)(-CHASSIS_SPEED_SLOW),
               &(chassis_go_opt_t){ .distance_cm = (DEMO_GO_CM * 0.5f), .straighten = false });
    if (!demo_wait_motion())
        return;
    if (!demo_pause())
        return;

    /* 6. 持续自旋 HOLD（+ 左） */
    Chassis_Turn(CHASSIS_TURN_SPEED_DEFAULT, 0.f, NULL);
    if (!demo_run_ms(DEMO_HOLD_MS))
        return;
    Chassis_Stop(CHASSIS_STOP_DEFAULT);
    if (!demo_run_ms(DEMO_PAUSE_MS))
        return;

    /* 7a. 定角左转 */
    Chassis_Turn(CHASSIS_TURN_SPEED_DEFAULT, DEMO_TURN_DEG, NULL);
    if (!demo_wait_motion())
        return;
    if (!demo_pause())
        return;

    /* 7b. 定角右转（回到大致朝向） */
    Chassis_Turn(CHASSIS_TURN_SPEED_DEFAULT, -DEMO_TURN_DEG, NULL);
    if (!demo_wait_motion())
        return;
    if (!demo_pause())
        return;

    /* 8. 巡线短时（需场地有线；否则丢线策略会停并关使能） */
    LineTrack_SetBaseSpeed(CHASSIS_SPEED_SLOW);
    LineTrack_SetEnable(true);
    {
        uint32_t t_prev = millis();
        uint32_t t0    = t_prev;
        while ((millis() - t0) < DEMO_LINE_MS) {
            uint32_t now = millis();
            uint32_t dt  = now - t_prev;
            if (dt < 5u)
                continue;
            if (dt > 50u)
                dt = 50u;
            t_prev = now;
            Chassis_Update(dt);
            if (!Chassis_Busy() && LineTrack_IsEnabled())
                LineTrack_Update();
            if (key_poll_press()) {
                LineTrack_SetEnable(false);
                Chassis_Abort();
                return;
            }
        }
    }
    LineTrack_SetEnable(false);
    Chassis_Stop(CHASSIS_STOP_DEFAULT);
    if (!demo_run_ms(DEMO_PAUSE_MS))
        return;

    /* 9. 清零里程后短定距，便于用 GetOdom 联调 */
    Chassis_ResetOdom();
    Chassis_ResetHeading();
    Chassis_Go(CHASSIS_SPEED_SLOW,
               &(chassis_go_opt_t){ .distance_cm = 15.f, .straighten = true });
    if (!demo_wait_motion())
        return;

    /* 10. 收尾 Abort → IDLE */
    Chassis_Abort();
    (void)demo_run_ms(DEMO_PAUSE_MS);
}
#endif

int main(void)
{
    uint32_t t_prev;
    uint32_t now;
    uint32_t dt;

    SYSCFG_DL_init();
    timebase_init();

    s_mbox.estop         = 0;
    s_mbox.remote_active = 0;
    s_mbox.throttle      = 0;
    s_mbox.turn          = 0;
    s_mbox.line_enable   = 0;

    Chassis_Init();
    LineTrack_Init();
    Chassis_Enable(true);

#if CAR_BOOT_DEMO
    /* 等板载 B21 单击后启动演示（安全：上电不立刻动） */
    key_wait_click();
    run_boot_demo();
#endif

    t_prev = millis();

    for (;;) {
        now = millis();
        dt  = now - t_prev;
        if (dt < 5u)
            continue;
        if (dt > 50u)
            dt = 50u;
        t_prev = now;

        Chassis_Update(dt);

        /* 主循环中 B21：再次演示（空闲时） */
        if (key_poll_press()) {
            if (!Chassis_Busy() && !s_mbox.remote_active && !s_mbox.estop) {
#if CAR_BOOT_DEMO
                LineTrack_SetEnable(false);
                run_boot_demo();
                t_prev = millis();
#endif
            } else {
                Chassis_Abort();
            }
            continue;
        }

        if (s_mbox.estop) {
            Chassis_Abort();
            Chassis_Enable(false);
            continue;
        }

        if (s_mbox.remote_active) {
            LineTrack_SetEnable(false);
            Chassis_Arcade(s_mbox.throttle, s_mbox.turn);
        } else if (Chassis_Busy()) {
            ;
        } else if (s_mbox.line_enable || LineTrack_IsEnabled()) {
            if (s_mbox.line_enable && !LineTrack_IsEnabled())
                LineTrack_SetEnable(true);
            LineTrack_Update();
        } else if (Chassis_GetState() == CHASSIS_STATE_IDLE) {
            Chassis_Stop(CHASSIS_STOP_DEFAULT);
        }
    }
}
