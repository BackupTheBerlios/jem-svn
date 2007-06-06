// Additional Copyrights:
//	None
//==============================================================================

#ifndef _JEMTYPES_H
#define _JEMTYPES_H

typedef signed char         jbyte;
typedef signed short        jshort;
typedef signed long         jint;
typedef signed long long    jlong;
typedef int                 jboolean;

struct ThreadDesc_s;
struct DomainDesc_s;
struct Proxy_s;
struct MethodInfoDesc_s;
struct DEPTypeDesc_s;
struct ServiceThreadPool_s;
struct DEPDesc_s;
struct Proxy_s;
struct CPUStateProxy_s;
struct AtomicVariableProxy_s;
struct CASProxy_s;
struct VMObjectProxy_s;
struct CredentialProxy_s;
struct DomainProxy_s;
struct InterceptOutboundInfo_s;
struct InterceptInboundInfoProxy_s;
struct InterceptPortalInfoProxy_s;
struct jarentry_s;
struct ObjectDesc_s;
struct ArrayDesc_s;
struct SymbolDesc_s;
struct SymbolDescStackMap_s;
struct FieldDesc_s;
struct ByteCodeDesc_s;
struct SourceLineDesc_s;
struct ExceptionDesc_s;
struct MethodDesc_s;
struct ClassDesc_s;
struct JClass_s;
struct SharedLibDesc_s;
struct LibDesc_s;
struct GCDesc_s;
struct ThreadDesc_s;
struct ThreadDescProxy_s;
struct ThreadDescForeignProxy_s;
struct MappedMemoryProxy_s;
struct StackProxy_s;
struct HLSchedDesc_s;
struct LLSchedDesc_s;
struct CpuDesc_s;
struct TempMemory_s;
struct profile_entry_s;
struct profile_s;

#define JNI_FALSE 0
#define JNI_TRUE 1

typedef void (*code_t) (void);
//typedef int (*int_code_t) ();
//typedef jlong(*longop_t) (jlong a, jlong b);

#ifndef CONFIG_SMP
    #define CONFIG_NR_CPUS 1
#endif
#define XMOFF 1

#define CLI_TEST_MODE 100

#endif              /* _JEMTYPES_H */
