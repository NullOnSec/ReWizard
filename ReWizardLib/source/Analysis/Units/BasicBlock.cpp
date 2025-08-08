#include <ReWizard/Analysis/Units/BasicBlock.h>

namespace ReWizard {


    BasicBlock::PtrBasicBlock BasicBlock::Split(uintptr_t address) {
        auto newBlock = BasicBlock::Create(func_, address);
        if (!newBlock) return nullptr;

        newBlock->SetEnd(endAddress_);
        endAddress_ = address; // set end of old block to split addr

        // The new block's predecessor is the old block (by address)
        newBlock->AddPredecessor(this->GetStart());

        // Move successors to new block
        for (auto& succ : successors_)
            newBlock->AddSuccessor(succ);

        successors_.clear();
        // Old block's new successor is the new block
        successors_.push_back(newBlock->GetStart());

        return newBlock;
    }



}

