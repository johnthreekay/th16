// Stand-ins for callees from other ranges whose bodies LTCG must see: the
// original callers keep values in registers across these calls, which LTCG
// only allows when it knows the callee leaves them alone. These are the
// real (small, recursive) bodies, kept out of line.
#include "../AnmManager.h"
#include "../AnmVm.h"
#include "../Supervisor.h"

// STUB: TH16 0x406a70
// The real body only touches pos itself, so LTCG's /GS analysis leaves
// callers that pass a local's address without a cookie. An opaque stub
// would give them one.
DECOMP_NOINLINE Float3 *AnmVm::transform_coords(Float3 *pos)
{
    pos->x *= g_screen_coord_scale;
    pos->y *= g_screen_coord_scale;
    pos->z *= g_screen_coord_scale;
    if (parent != NULL && !(flags_hi & ANM_VM_NO_PARENT_POS))
    {
        Float3 offset;
        parent->get_own_transformed_pos(&offset);
        pos->x += offset.x;
        pos->y += offset.y;
        pos->z += offset.z;
    }
    return pos;
}
