
#ifndef __BITS_H__
#define __BITS_H__

#ifdef __cplusplus
extern "C"
{
#endif

static inline void bits_w8(uint8_t **s, uint32_t val)
{
    **s = val;
    *s += 1; 
}

static inline void bits_wb16(uint8_t **s, uint32_t val)
{
    bits_w8(s, (int)val >> 8);
    bits_w8(s, (uint8_t)val);

}

static inline void bits_wb24(uint8_t **s, uint32_t val)
{
    bits_w8(s, (uint8_t)(val >> 16));
    bits_w8(s, (int)val >> 8);
    bits_w8(s, (uint8_t)val);
}

static inline void bits_wb32(uint8_t **s, uint32_t val)
{
    bits_w8(s,           val >> 24 ); 
    bits_w8(s, (uint8_t)(val >> 16));
    bits_w8(s, (uint8_t)(val >> 8 ));
    bits_w8(s, (uint8_t) val       );
}

static inline void bits_write(uint8_t **s, uint8_t *val, uint32_t size)
{
    memcpy(*s, val, size);
    *s += size;
}

#ifdef __cplusplus
}

#endif


#endif

