#ifndef BOCHS_INSTRUMENTATION_BRIDGE_H
#define BOCHS_INSTRUMENTATION_BRIDGE_H

#include <cstdint>
#include <cstddef>

namespace ReWizard {

    class ITraceProducer;

    // Set the trace producer that will receive instrumentation callbacks from Bochs.
    // Call this before starting Bochs execution.
    void ReWizard_SetBochsTraceProducer(ITraceProducer* producer);

    // Activate/deactivate trace collection.
    void ReWizard_SetBochsTraceActive(bool active);

}

#endif
