#include <ReWizard/Analysis/PassManager.h>

namespace ReWizard {

    void AnalysisPassManager::RunAllAsync(AnalysisContext* ctx) {
        std::promise<void> prom;
        std::future<void> fut = prom.get_future();

        std::thread t(
            [this, ctx, p = std::move(prom)] () mutable {
                for (auto& pass : m_passes) {
                    if (!pass->PreRun(ctx))
                        break;

                    if (!pass->Run(ctx))
                        break;

                    if (!pass->PostRun(ctx))
                        break;
                }
                p.set_value();
            }
        );

        t.detach();
        fut.wait();
    }

}
