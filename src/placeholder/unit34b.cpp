// Stand-ins for callees from other ranges whose bodies LTCG must see: the
// original callers keep values in registers across these calls, which LTCG
// only allows when it knows the callee leaves them alone. These are the
// real (small, recursive) bodies, kept out of line.
#include "../AnmManager.h"
#include "../AnmVm.h"
#include "../Supervisor.h"

