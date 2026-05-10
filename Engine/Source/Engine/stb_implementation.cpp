// stb_image (decode) is needed at runtime for Texture::LoadFromMemory
// (HTTP-response → Texture and friends). The other stb libs stay editor-only
// — they're only used by the editor's import / export / font-rasterise paths.
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#if EDITOR
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize2.h>
#endif
