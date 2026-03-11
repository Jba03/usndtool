#include "vm.h"

usnd_size usnd_program_size(const struct CProgramHeader *hdr) {
  if (hdr->fn_descriptor_pos == 0xFFFFFFFF) return 0;
  return hdr->fn_descriptor_pos + hdr->fn_descriptor_size;
}
