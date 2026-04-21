#include "logging.h"

#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>

#include <cstdio>

namespace gwheel::log
{
    const char* HresultName(long hr)
    {
        switch (static_cast<HRESULT>(hr))
        {
        case S_OK:                          return "S_OK";
        case S_FALSE:                       return "S_FALSE";
        case E_FAIL:                        return "E_FAIL";
        case E_INVALIDARG:                  return "E_INVALIDARG";
        case E_OUTOFMEMORY:                 return "E_OUTOFMEMORY";
        case E_NOINTERFACE:                 return "E_NOINTERFACE";
        case E_POINTER:                     return "E_POINTER";
        case E_ACCESSDENIED:                return "E_ACCESSDENIED";
        case E_HANDLE:                      return "E_HANDLE";
        case DIERR_INPUTLOST:               return "DIERR_INPUTLOST";
        case DIERR_NOTACQUIRED:             return "DIERR_NOTACQUIRED";
        case DIERR_NOTINITIALIZED:          return "DIERR_NOTINITIALIZED";
        case DIERR_UNSUPPORTED:             return "DIERR_UNSUPPORTED";
        case DIERR_OUTOFMEMORY:             return "DIERR_OUTOFMEMORY";
        case DIERR_DEVICENOTREG:            return "DIERR_DEVICENOTREG";
        case DIERR_BETADIRECTINPUTVERSION:  return "DIERR_BETADIRECTINPUTVERSION";
        case DIERR_OLDDIRECTINPUTVERSION:   return "DIERR_OLDDIRECTINPUTVERSION";
        case DIERR_INVALIDPARAM:            return "DIERR_INVALIDPARAM";
        case DIERR_NOAGGREGATION:           return "DIERR_NOAGGREGATION";
        case DIERR_NOTFOUND:                return "DIERR_NOTFOUND";
        case DIERR_OTHERAPPHASPRIO:         return "DIERR_OTHERAPPHASPRIO";
        case DIERR_HASEFFECTS:              return "DIERR_HASEFFECTS";
        case DIERR_DEVICEFULL:              return "DIERR_DEVICEFULL";
        case DIERR_MOREDATA:                return "DIERR_MOREDATA";
        case DIERR_NOTDOWNLOADED:           return "DIERR_NOTDOWNLOADED";
        case DIERR_NOTEXCLUSIVEACQUIRED:    return "DIERR_NOTEXCLUSIVEACQUIRED";
        case DIERR_INCOMPLETEEFFECT:        return "DIERR_INCOMPLETEEFFECT";
        case DIERR_ACQUIRED:                return "DIERR_ACQUIRED";
        case DIERR_GENERIC:                 return "DIERR_GENERIC";
        default: break;
        }
        static thread_local char buf[16];
        std::snprintf(buf, sizeof(buf), "0x%08lX", hr);
        return buf;
    }
}
