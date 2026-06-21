#pragma once

#include <helpers/ui/UIScreen.h>
#include <stdint.h>

class UITask;

// Trace-route tool: pick a reachable contact, send a trace over its direct
// path, then show each hop's SNR plus the final hop-to-us SNR.
class TraceRouteScreen : public UIScreen {
public:
  struct Target { char name[24]; int idx; };  // idx = contact index for the task

private:
  enum State { PICK, TRACING, RESULT, FAILED };
  UITask* _task;
  uint32_t _now;

  static const int MAX_TARGETS = 24;
  Target _targets[MAX_TARGETS];
  int _ntargets, _sel, _scroll_top;

  // active trace
  uint32_t _tag;
  uint32_t _start_ts;
  char _target_name[24];
  char _fail_msg[40];

  // result
  static const int MAX_HOPS = 16;
  uint8_t _hop_hash[MAX_HOPS];
  int8_t  _hop_snr[MAX_HOPS];   // SNR*4 per hop
  int _nhops;
  int8_t _final_snr;            // SNR*4 to us

  State _state;
  int _last_y; bool _moved, _pressing;

  int renderPick(DisplayDriver& d);
  int renderStatus(DisplayDriver& d);   // TRACING / FAILED
  int renderResult(DisplayDriver& d);

public:
  TraceRouteScreen(UITask* task)
      : _task(task), _now(0), _ntargets(0), _sel(0), _scroll_top(0),
        _tag(0), _start_ts(0), _nhops(0), _final_snr(0), _state(PICK),
        _last_y(0), _moved(false), _pressing(false) {
    _target_name[0] = 0; _fail_msg[0] = 0;
  }

  void setNow(uint32_t now) { _now = now; }
  void beginPick() { _state = PICK; _sel = 0; _scroll_top = 0; }
  void clearTargets() { _ntargets = 0; }
  void addTarget(const char* name, int idx);

  // called by the task after attempting to start a trace
  void onTraceStarted(uint32_t tag, const char* target_name);
  void onTraceFailed(const char* reason);
  // called from UITask::onTraceResult
  void onResult(uint32_t tag, const uint8_t* hashes, const uint8_t* snrs,
                uint8_t path_len, uint8_t path_sz, int8_t final_snr_q);

  int render(DisplayDriver& display) override;
  bool handleInput(char c) override;
  bool handleTouch(int x, int y, TouchEvent ev) override;
};
