import argparse
import locale
import os
import re
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path


DEFAULT_TARGET = r"D:\TI\mspm0_sdk_2_05_01_00\tools\keil\syscfg.bat"


@dataclass
class FileEncoding:
	encoding: str
	bom: bytes


def _detect_encoding(raw: bytes) -> FileEncoding:
	if raw.startswith(b"\xef\xbb\xbf"):
		return FileEncoding("utf-8-sig", b"\xef\xbb\xbf")
	if raw.startswith(b"\xff\xfe"):
		return FileEncoding("utf-16-le", b"\xff\xfe")
	if raw.startswith(b"\xfe\xff"):
		return FileEncoding("utf-16-be", b"\xfe\xff")
	return FileEncoding(locale.getpreferredencoding(False), b"")


def _decode_text(raw: bytes, encoding: FileEncoding) -> str:
	try:
		return raw.decode(encoding.encoding)
	except UnicodeDecodeError:
		if encoding.encoding.lower() != "utf-8":
			return raw.decode("utf-8")
		raise


def _normalize_path(path_text: str) -> str:
	cleaned = path_text.strip().strip("\"")
	return cleaned.replace("/", "\\")


def _line_ending(line: str) -> str:
	if line.endswith("\r\n"):
		return "\r\n"
	if line.endswith("\n"):
		return "\n"
	if line.endswith("\r"):
		return "\r"
	return ""


def _confirm_change(original: str, proposed: str) -> bool:
	print("\n将要修改以下行:")
	print("- 原始:", original)
	print("- 新值:", proposed)
	while True:
		answer = input("是否应用该修改? (y/n): ").strip().lower()
		if answer in {"y", "yes"}:
			return True
		if answer in {"n", "no"}:
			return False
		print("请输入 y 或 n。")


def _build_backup_path(target: Path) -> Path:
	timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
	return target.with_suffix(target.suffix + f".bak.{timestamp}")


def update_syscfg_path(target: Path, new_path: str) -> int:
	if not target.exists():
		print(f"文件不存在: {target}")
		return 1

	raw = target.read_bytes()
	encoding = _detect_encoding(raw)
	text = _decode_text(raw, encoding)

	lines = text.splitlines(keepends=True)
	pattern = re.compile(r"(\s*set\s+SYSCFG_PATH=)(.*)", re.IGNORECASE)
	normalized = _normalize_path(new_path)
	replaced = False
	updated_lines = []

	for line in lines:
		match = pattern.match(line)
		if not match:
			updated_lines.append(line)
			continue

		ending = _line_ending(line)
		original = line.rstrip("\r\n")
		proposed = f"{match.group(1)}\"{normalized}\""

		if _confirm_change(original, proposed):
			updated_lines.append(proposed + ending)
			replaced = True
		else:
			updated_lines.append(line)

	if not replaced:
		print("未进行任何修改。")
		return 2

	backup_path = _build_backup_path(target)
	backup_path.write_bytes(raw)

	new_text = "".join(updated_lines)
	target.write_bytes(encoding.bom + new_text.encode(encoding.encoding))

	print(f"已更新: {target}")
	print(f"备份已创建: {backup_path}")
	return 0


def main() -> int:
	parser = argparse.ArgumentParser(
		description="更新 syscfg.bat 中的 SYSCFG_PATH 路径（带备份与逐条确认）。"
	)
	parser.add_argument(
		"-f",
		"--file",
		default=DEFAULT_TARGET,
		help="要修改的 syscfg.bat 文件路径。",
	)
	parser.add_argument(
		"-p",
		"--path",
		default="",
		help="新的 sysconfig_cli.bat 路径（不填则运行时输入）。",
	)

	args = parser.parse_args()
	target = Path(os.path.expandvars(args.file)).resolve()
	new_path = args.path.strip()

	if not new_path:
		new_path = input("请输入新的 sysconfig_cli.bat 路径: ").strip()

	if not new_path:
		print("未提供新路径，已退出。")
		return 3

	return update_syscfg_path(target, new_path)


if __name__ == "__main__":
	raise SystemExit(main())
