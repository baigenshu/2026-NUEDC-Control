#include "avi_mjpeg.h"

static uint8_t s_sdCs = 13;

void AviMjpegWriter::writeU16(uint16_t v)
{
  _file.write((uint8_t)(v & 0xFF));
  _file.write((uint8_t)((v >> 8) & 0xFF));
}

void AviMjpegWriter::writeU32(uint32_t v)
{
  _file.write((uint8_t)(v & 0xFF));
  _file.write((uint8_t)((v >> 8) & 0xFF));
  _file.write((uint8_t)((v >> 16) & 0xFF));
  _file.write((uint8_t)((v >> 24) & 0xFF));
}

void AviMjpegWriter::writeFourCC(const char *s)
{
  _file.write((const uint8_t *)s, 4);
}

void AviMjpegWriter::patchU32(uint32_t pos, uint32_t v)
{
  uint32_t cur = _file.position();
  _file.seek(pos);
  writeU32(v);
  _file.seek(cur);
}

bool AviMjpegWriter::begin(const char *path, uint16_t width, uint16_t height, uint16_t fpsHint)
{
  if (_open)
    end();

  strncpy(_path, path, sizeof(_path) - 1);
  _path[sizeof(_path) - 1] = 0;
  _w = width;
  _h = height;
  _fps = fpsHint ? fpsHint : 15;
  _frames = 0;
  _moviSize = 4; // "movi"
  _maxFrameBytes = 0;

  // FILE_WRITE truncates/creates on ESP32 SD
  _file = SD.open(_path, FILE_WRITE);
  if (!_file)
  {
    Serial.printf("AVI open fail: %s\n", _path);
    return false;
  }

  // --- RIFF AVI ---
  // strh data = 56, strf data = 40
  // strl LIST size = "strl"(4) + strh(8+56) + strf(8+40) = 116
  // hdrl LIST size = "hdrl"(4) + avih(8+56) + LIST strl(8+116) = 192
  writeFourCC("RIFF");
  _posRiffSize = _file.position();
  writeU32(0);
  writeFourCC("AVI ");

  writeFourCC("LIST");
  writeU32(192);
  writeFourCC("hdrl");

  // avih (56 bytes)
  writeFourCC("avih");
  writeU32(56);
  uint32_t usec = 1000000UL / _fps;
  writeU32(usec);
  writeU32(0);
  writeU32(0);
  writeU32(0x10); // AVIF_TRUSTCKTYPE
  _posHdrlAvihFrames = _file.position();
  writeU32(0); // dwTotalFrames
  writeU32(0);
  writeU32(1);
  writeU32(0);
  writeU32(_w);
  writeU32(_h);
  writeU32(0);
  writeU32(0);
  writeU32(0);
  writeU32(0);

  writeFourCC("LIST");
  writeU32(116);
  writeFourCC("strl");

  // strh (56 bytes AVISTREAMHEADER)
  writeFourCC("strh");
  writeU32(56);
  writeFourCC("vids");
  writeFourCC("MJPG");
  writeU32(0);
  writeU16(0);
  writeU16(0);
  writeU32(0);
  writeU32(1);    // scale
  writeU32(_fps); // rate
  writeU32(0);
  _posStrhLength = _file.position();
  writeU32(0); // length in frames
  writeU32(0);
  writeU32(-1); // quality
  writeU32(0);  // sample size (0 = variable)
  writeU16(0);
  writeU16(0);
  writeU16(_w);
  writeU16(_h);

  // strf BITMAPINFOHEADER (40)
  writeFourCC("strf");
  writeU32(40);
  writeU32(40);
  writeU32(_w);
  writeU32(_h);
  writeU16(1);
  writeU16(24);
  writeFourCC("MJPG");
  writeU32(_w * _h * 3);
  writeU32(0);
  writeU32(0);
  writeU32(0);
  writeU32(0);

  writeFourCC("LIST");
  _posMoviListSize = _file.position();
  writeU32(0);
  writeFourCC("movi");
  _posMoviData = _file.position();

  _open = true;
  Serial.printf("AVI rec start: %s %ux%u @%ufps\n", _path, _w, _h, _fps);
  return true;
}

bool AviMjpegWriter::writeFrame(const uint8_t *jpeg, uint32_t jpegLen)
{
  if (!_open || !jpeg || jpegLen < 4)
    return false;

  // chunk must be word-aligned
  uint32_t pad = jpegLen & 1;
  uint32_t chunkDataSize = jpegLen + pad;

  writeFourCC("00dc");
  writeU32(chunkDataSize);
  size_t n = _file.write(jpeg, jpegLen);
  if (n != jpegLen)
  {
    Serial.println("AVI write short");
    return false;
  }
  if (pad)
    _file.write((uint8_t)0);

  _moviSize += 8 + chunkDataSize;
  _frames++;
  if (jpegLen > _maxFrameBytes)
    _maxFrameBytes = jpegLen;

  // periodic flush to reduce corruption on power loss
  if ((_frames % 15) == 0)
    _file.flush();

  return true;
}

bool AviMjpegWriter::end()
{
  if (!_open)
    return true;

  // optional idx1 skipped for simplicity (many players still play)

  uint32_t fileSize = _file.position();
  patchU32(_posRiffSize, fileSize - 8);
  patchU32(_posHdrlAvihFrames, _frames);
  patchU32(_posStrhLength, _frames);
  // movi LIST size = "movi"(4) + chunks = _moviSize
  patchU32(_posMoviListSize, _moviSize);

  _file.flush();
  _file.close();
  _open = false;
  Serial.printf("AVI rec stop: %s frames=%u size=%u\n", _path, _frames, (unsigned)fileSize);
  return true;
}

bool sdInitSharedSpi(uint8_t sdCs, int32_t spiHz)
{
  s_sdCs = sdCs;
  pinMode(sdCs, OUTPUT);
  digitalWrite(sdCs, HIGH);

  // Keep TFT CS high while talking to SD
  // Caller should ensure TFT_CS is HIGH

  if (!SD.begin(sdCs, SPI, spiHz))
  {
    Serial.printf("SD.begin failed CS=%u\n", sdCs);
    return false;
  }

  if (!SD.exists("/REC"))
  {
    if (!SD.mkdir("/REC"))
    {
      Serial.println("mkdir /REC failed");
      // continue; open may still work on root
    }
  }

  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE)
  {
    Serial.println("No SD card");
    return false;
  }
  Serial.printf("SD OK type=%u size=%lluMB\n", cardType, SD.cardSize() / (1024ULL * 1024ULL));
  return true;
}

uint16_t nextRecIndex()
{
  uint16_t idx = 1;
  char name[28];
  while (idx < 10000)
  {
    snprintf(name, sizeof(name), "/REC/VID_%04u.avi", idx);
    if (!SD.exists(name))
      return idx;
    idx++;
  }
  return 1;
}

bool openNextRecording(AviMjpegWriter &avi, uint16_t w, uint16_t h, uint16_t fps)
{
  uint16_t idx = nextRecIndex();
  char path[28];
  snprintf(path, sizeof(path), "/REC/VID_%04u.avi", idx);
  return avi.begin(path, w, h, fps);
}
