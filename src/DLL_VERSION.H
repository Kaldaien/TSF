#pragma once


#define TSF_MAJOR 0
#define TSF_MINOR 10
#define TSF_BUILD 4
#define TSF_REV   0





#define _A2(a)     #a
#define  _A(a)  _A2(a)
#define _L2(w)  L ## w
#define  _L(w) _L2(w)


#if (TSF_REV > 0)
#define TSF_VERSION_STR_A    _A(TSF_MAJOR) "." _A(TSF_MINOR) "." _A(TSF_BUILD) "." _A(TSF_REV)
#else
#define TSF_VERSION_STR_A    _A(TSF_MAJOR) "." _A(TSF_MINOR) "." _A(TSF_BUILD)
#endif

#define TSF_VERSION_STR_W _L(TSF_VERSION_STR_A)


#define TSF_FILE_VERSION     TSF_MAJOR,TSF_MINOR,TSF_BUILD,TSF_REV
#define TSF_PRODUCT_VERSION  TSF_MAJOR,TSF_MINOR,TSF_BUILD,TSF_REV
