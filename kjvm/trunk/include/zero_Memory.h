// Additional Copyrights:
// 	Copyright (C) 1997-2001 The JX Group.
//==============================================================================

#ifndef ZERO_MEMORY_H
#define ZERO_MEMORY_H


typedef struct MemoryProxy_s {
    code_t  *vtable;
    u32     size;
    char    *mem;
} MemoryProxy;

typedef struct MemoryProxy_s **MemoryProxyHandle;

void dzmemory_decRefcount(struct MemoryProxy_s *m);
void dzmemory_alive(struct MemoryProxy_s *dzm);
void dzmemory_redirect_invalid_dz(MemoryProxyHandle mem);


ObjectDesc *copy_memory(struct DomainDesc_s *src, struct DomainDesc_s *dst,
                        struct MemoryProxy_s *obj, u32 * quota);


MemoryProxyHandle allocMemoryProxyInDomain(DomainDesc * domain,
                                           ClassDesc * c, jint start,
                                           jint size);

struct MemoryProxy_s;
struct MemoryProxy_s *gc_impl_shallowCopyMemory(u32 * dst,
                                                struct MemoryProxy_s
                                                *srcObj);

u32             memory_sizeof_proxy(void);
void            memory_deleted(struct MemoryProxy_s *obj);
jint            memory_getStartAddress(ObjectDesc * self);
jint            memory_size(ObjectDesc * self);
ObjectDesc      *memoryManager_alloc(ObjectDesc * self, jint size);
ObjectDesc      *memoryManager_allocAligned(ObjectDesc * self, jint size, jint bytes);
ObjectDesc      *copy_shallow_memory(DomainDesc * src, DomainDesc * dst, struct MemoryProxy_s * obj, u32 * quota);
void            copy_content_memory(DomainDesc * src, DomainDesc * dst, struct MemoryProxy_s * obj, u32 * quota);

MemoryProxyHandle allocReadOnlyMemory(MemoryProxy * self, jint start, jint size);

#endif
