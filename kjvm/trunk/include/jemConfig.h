//
// Jem/JVM configuration.
//
//===========================================================================

#ifndef _JEMCONFIG_H
#define _JEMCONFIG_H

enum				confIds   {	codeFragments,
								maxServices,
								maxDomains,
								maxNumberLibs,
								domScratchMemSz,
								heapBytesDom0,
								codeBytesDom0,
								codeBytes,
								jemConfSize
							  };

int getJVMConfig(unsigned int id);


#endif

