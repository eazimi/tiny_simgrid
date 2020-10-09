#include "CheckerSide.h"

namespace uc 
{
    void CheckerSide::run(std::shared_ptr<AppSide> app_side)
    {
        uc_->explore(app_side);
    }
} // namespace uc