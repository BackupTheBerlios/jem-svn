//
// Jem/JVM configuration.
//
//===========================================================================

#ifndef _JEMCONFIG_H
#define _JEMCONFIG_H

struct jvmConfig {
    unsigned int    maxDomains;
    unsigned int    heapBytesDom0;
    unsigned int    codeBytesDom0;
    unsigned int    maxNumberLibs;
    unsigned int    domScratchMemSz;
    unsigned int    maxRegistered;
    unsigned int    maxHeapSamples;
    unsigned int    heapReserve;
    unsigned int    defaultServicePrio;
    unsigned int    defaultThreadPrio;
    unsigned int    defaultDomainPrio;
    unsigned int    serviceStackSz;
    unsigned int    serviceParamsSz;
    unsigned int    receivePortalQuota;
    unsigned int    createDomainPortalQuota;
    unsigned int	codeFragments;
};


struct jvmConfig *getJVMConfig(void);


#endif

