// PSRAM-backed LVGL allocator (LV_USE_STDLIB_MALLOC == LV_STDLIB_CUSTOM).
// Widget trees, styles and other lv_malloc allocations land in the 8MB PSRAM,
// keeping the scarce internal DRAM heap free for the Wi-Fi stack, lwip and DMA
// buffers. Falls back to the internal heap if a PSRAM allocation fails. The
// draw buffers are separate statics in UITask.cpp and stay in internal RAM
// (render-path performance). Mirrors lvgl/src/stdlib/clib/lv_mem_core_clib.c.
#include <lvgl.h>
#if LV_USE_STDLIB_MALLOC == LV_STDLIB_CUSTOM
#include <esp_heap_caps.h>
#include <stdlib.h>

void lv_mem_init(void) {}
void lv_mem_deinit(void) {}
lv_mem_pool_t lv_mem_add_pool(void* mem, size_t bytes) { LV_UNUSED(mem); LV_UNUSED(bytes); return NULL; }
void lv_mem_remove_pool(lv_mem_pool_t pool) { LV_UNUSED(pool); }

void* lv_malloc_core(size_t size) {
  void* p = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
  return p ? p : malloc(size);
}
void* lv_realloc_core(void* p, size_t new_size) {
  void* n = heap_caps_realloc(p, new_size, MALLOC_CAP_SPIRAM);
  return n ? n : realloc(p, new_size);
}
void lv_free_core(void* p) { free(p); }

void lv_mem_monitor_core(lv_mem_monitor_t* mon_p) { LV_UNUSED(mon_p); }
lv_result_t lv_mem_test_core(void) { return LV_RESULT_OK; }

#endif /*LV_STDLIB_CUSTOM*/
