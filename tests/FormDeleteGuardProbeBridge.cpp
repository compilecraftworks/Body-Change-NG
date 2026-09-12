#include "BodyChangeNG/RaceMenuFormDeleteGuard.h"
extern "C" __declspec(dllexport) bool GuardInstall(void* module,bool verifiedHandleLayout)
{ return bcn::racemenu_form_delete::Install(module,verifiedHandleLayout); }
extern "C" __declspec(dllexport) const char* GuardStatus()
{ return bcn::racemenu_form_delete::Status(); }
extern "C" __declspec(dllexport) void GuardStats(bcn::racemenu_form_delete::Stats* stats)
{ if (stats) *stats=bcn::racemenu_form_delete::Statistics(); }
