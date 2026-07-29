/*
  Minimal AVI writer for MJPEG frames (VLC-playable).
  Shared SPI: call begin/end only when TFT CS is idle (HIGH).
*/
#pragma once

#include <Arduino.h>
#include <FS.h>
#include <SD.h>

class AviMjpegWriter
{
public:
  bool begin(const char *path, uint16_t width, uint16_t height, uint16_t fpsHint = 15);
  bool writeFrame(const uint8_t *jpeg, uint32_t jpegLen);
  bool end();
  bool isOpen() const { return _open; }
  uint32_t frameCount() const { return _frames; }
  const char *path() const { return _path; }

private:
  File _file;
  bool _open = false;
  char _path[40];
  uint16_t _w = 0;
  uint16_t _h = 0;
  uint16_t _fps = 15;
  uint32_t _frames = 0;
  uint32_t _moviSize = 4; // "movi" already counted in LIST size math
  uint32_t _maxFrameBytes = 0;
  uint32_t _posRiffSize = 0;
  uint32_t _posHdrlAvihFrames = 0;
  uint32_t _posStrhLength = 0;
  uint32_t _posMoviListSize = 0;
  uint32_t _posMoviData = 0;

  void writeU16(uint16_t v);
  void writeU32(uint32_t v);
  void writeFourCC(const char *s);
  void patchU32(uint32_t pos, uint32_t v);
};

bool sdInitSharedSpi(uint8_t sdCs, int32_t spiHz = 20000000);
uint16_t nextRecIndex();
bool openNextRecording(AviMjpegWriter &avi, uint16_t w, uint16_t h, uint16_t fps);
