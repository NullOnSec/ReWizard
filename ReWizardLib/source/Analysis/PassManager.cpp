#include <ReWizard/Analysis/PassManager.h>

namespace ReWizard {

    void AnalysisPassManager::RunAllAsync(AnalysisContext* ctx) {
        std::promise<bool> prom;
        std::future<bool> fut = prom.get_future();

        std::thread _(
            [this, ctx, p = std::move(prom)] () mutable {
                bool ok = false;
                for (auto& [_, pass] : GetAllPasses()) {
                    if (pass) {

                        if (!(ok = pass->PreRun(ctx))) break;

                        if (!(ok = pass->Run(ctx))) break;

                        if (!(ok = pass->PostRun(ctx))) break;

                    }
                }
                p.set_value(ok);
            }
        );

        _.detach();
        fut.wait();
    }

    std::future<bool> AnalysisPassManager::RunAsync(AnalysisContext* ctx, BaseAnalysisPass* pass) {
        std::promise<bool> prom;
        auto fut = prom.get_future();

        std::thread(
            [ctx, pass, p = std::move(prom)]() mutable {
                bool ok = false;
                if (pass) {
                    ok = pass->PreRun(ctx);
                    if (ok) ok = pass->Run(ctx);
                    if (ok) ok = pass->PostRun(ctx);
                }
                p.set_value(ok);
            }
        ).detach();

        return fut;
    }

}
