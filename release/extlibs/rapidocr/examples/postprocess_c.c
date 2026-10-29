#include <stdio.h>
#include <string.h>
#include "xrt.h"
#include "rapidocr.h"

int main(void)
{
    const char* boxes = "[{\"box\":[[0,0],[10,0],[10,4],[0,4]]}]";
    char* result = rapidocr_assemble_result_native(boxes,
        "{\"text\":\"HELLO123\",\"score\":0.99}", 0.0);
    char* items = result != NULL ? rapidocr_to_ocr_items_json_native(result) : NULL;
    void* geometry = rapidocr_boxes_open_native(boxes);
    int ok = geometry != NULL && rapidocr_boxes_count_handle_native(geometry) == 1 &&
        rapidocr_boxes_coord_native(geometry, 0, 2) == 10.0 &&
        result != NULL && items != NULL &&
        strstr(result, "\"HELLO123\"") != NULL &&
        strstr(items, "\"HELLO123\"") != NULL;
    rapidocr_boxes_release_native(geometry);
    xrtFree(items);
    xrtFree(result);
    if (!ok) return 1;
    puts("rapidocr postprocess ok");
    return 0;
}
