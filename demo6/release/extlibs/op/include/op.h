#ifndef XL_EXTLIB_OP_H
#define XL_EXTLIB_OP_H

#ifdef __cplusplus
extern "C" {
#endif

/* Windows-only C API. 句柄由 create 创建、destroy 释放。 */
void *xop_create(void);
void xop_destroy(void *context);
/* 空模式采用 normal；display 支持 normal/gdi/dxgi/opengl/d3d9/d3d11/wgl，
   mouse/keypad 支持 normal/windows，mode 当前仅允许 0。无效参数不改变旧绑定。 */
long long xop_bind_window(void *context, long long hwnd, const char *display,
                         const char *mouse, const char *keypad, long long mode);
long long xop_unbind(void *context);
long long xop_is_bind(void *context);
long long xop_get_bind_window(void *context);

/* 文本由 XRT 独立分配，C 调用方用 xrtFree 释放。 */
char *xop_get_color(void *context, long long x, long long y);
char *xop_get_colors(void *context, long long x1, long long y1,
                     long long x2, long long y2);
long long xop_capture(void *context, long long x1, long long y1,
                      long long x2, long long y2, const char *file);
char *xop_get_frame_info(void *context);
char *xop_find_color(void *context, long long x1, long long y1,
                      long long x2, long long y2, const char *color,
                      double similarity, long long direction);
char *xop_find_color_ex(void *context, long long x1, long long y1,
                        long long x2, long long y2, const char *color,
                        double similarity, long long direction);
char *xop_find_multi_color(void *context, long long x1, long long y1,
                            long long x2, long long y2,
                            const char *first_color, const char *offset_color,
                            double similarity, long long direction);
char *xop_find_pic(void *context, long long x1, long long y1,
                    long long x2, long long y2, const char *file,
                    const char *delta_color, double similarity,
                    long long direction);
char *xop_find_pic_ex(void *context, long long x1, long long y1,
                       long long x2, long long y2, const char *file,
                       const char *delta_color, double similarity,
                       long long direction);
char *xop_find_color_block(void *context, long long x1, long long y1,
                            long long x2, long long y2, const char *color,
                            double similarity, long long width,
                            long long height);
long long xop_get_color_num(void *context, long long x1, long long y1,
                            long long x2, long long y2, const char *color,
                            double similarity);
char *xop_find_pic_n(void *context, long long x1, long long y1,
                      long long x2, long long y2, const char *files,
                      const char *delta_color, double similarity,
                      long long direction);
char *xop_capture_to_mem(void *context, long long x1, long long y1,
                         long long x2, long long y2);
double xop_image_compare(void *context, long long x1, long long y1,
                          long long x2, long long y2, const char *file,
                          double similarity);
char *xop_image_hash(void *context, long long x1, long long y1,
                     long long x2, long long y2);

/* 输入操作会影响当前桌面，调用方须显式选择何时执行。 */
long long xop_move_to(void *context, long long x, long long y);
long long xop_left_click(void *context);
long long xop_right_click(void *context);
long long xop_middle_click(void *context);
long long xop_left_down(void *context);
long long xop_left_up(void *context);
long long xop_right_down(void *context);
long long xop_right_up(void *context);
long long xop_wheel_up(void *context);
long long xop_wheel_down(void *context);
long long xop_drag_to(void *context, long long x1, long long y1,
                       long long x2, long long y2);
long long xop_key_press(void *context, long long key);
long long xop_key_down(void *context, long long key);
long long xop_key_up(void *context, long long key);
long long xop_key_press_str(void *context, const char *text, long long delay);
long long xop_key_char(void *context, long long codepoint);
long long xop_move_to_smooth(void *context, long long x, long long y,
                             long long steps, long long delay);
long long xop_drag_path(void *context, const char *points, long long delay);
long long xop_key_combo(void *context, const char *combo);

/* 窗口、进程与工具接口。整数窗口值承载 HWND，不是跨进程持久 ID。 */
long long xop_find_window(const char *class_name, const char *title);
char *xop_enum_window(long long parent, const char *title,
                      const char *class_name, long long filter);
char *xop_enum_window_by_process(const char *process,
                                 const char *title,
                                 const char *class_name, long long filter);
/* 本组文本返回独立的 XRT 字符串；C 调用方用 xrtFree 释放。 */
char *xop_get_screen_size(void);
char *xop_get_cursor_pos(void);
char *xop_get_window_rect(long long hwnd);
long long xop_move_window(long long hwnd, long long x, long long y);
long long xop_set_window_size(long long hwnd, long long width, long long height);
char *xop_get_window_title(long long hwnd);
long long xop_set_window_title(long long hwnd, const char *title);
long long xop_set_window_state(long long hwnd, long long mode);
long long xop_send_string(long long hwnd, const char *text);
char *xop_client_to_screen(long long hwnd, long long x, long long y);
char *xop_screen_to_client(long long hwnd, long long x, long long y);
long long xop_read_int(long long pid, long long address);
double xop_read_float(long long pid, long long address);
char *xop_read_string(long long pid, long long address, long long length);
/* 写入目标是 C int32/float32；无法表示或非有限值报错，不修改目标。 */
long long xop_write_int(long long pid, long long address, long long value);
long long xop_write_float(long long pid, long long address, double value);
long long xop_write_string(long long pid, long long address, const char *value);
char *xop_find_memory(long long pid, long long start, long long end,
                      const char *pattern);
char *xop_enum_process(const char *filter);
char *xop_get_process_modules(long long pid);
void xop_delay(long long milliseconds);
long long xop_random_int(long long min, long long max);
long long xop_set_clipboard(const char *text);
char *xop_get_clipboard(void);

#ifdef __cplusplus
}
#endif

#endif
