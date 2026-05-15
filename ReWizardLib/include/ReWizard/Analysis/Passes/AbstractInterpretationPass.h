#ifndef ABSTRACT_INTERPRETATION_PASS_H
#define ABSTRACT_INTERPRETATION_PASS_H

#include <ReWizard/Analysis/PassProvider.h>
#include <Zydis/Zydis.h>
#include <cstdint>
#include <map>
#include <optional>

namespace ReWizard {
    class AnalysisContext;
    class Function;
    class BasicBlock;
    class ExtendedInstruction;

    struct Interval {
        int64_t lo = 0;
        int64_t hi = 0;
        bool isBottom = true; // uninitialized / contradiction

        static Interval Top() { Interval i; i.lo = INT64_MIN; i.hi = INT64_MAX; i.isBottom = false; return i; }
        static Interval Constant(int64_t v) { Interval i; i.lo = v; i.hi = v; i.isBottom = false; return i; }
        static Interval Range(int64_t l, int64_t h) { Interval i; i.lo = l; i.hi = h; i.isBottom = false; return i; }

        bool IsConstant() const { return !isBottom && lo == hi; }
        bool Contains(int64_t v) const { return !isBottom && v >= lo && v <= hi; }

        Interval Join(const Interval& other) const {
            if (isBottom) return other;
            if (other.isBottom) return *this;
            return Range(std::min(lo, other.lo), std::max(hi, other.hi));
        }

        Interval Add(const Interval& other) const {
            if (isBottom || other.isBottom) return Interval();
            return Range(lo + other.lo, hi + other.hi);
        }

        Interval Sub(const Interval& other) const {
            if (isBottom || other.isBottom) return Interval();
            return Range(lo - other.hi, hi - other.lo);
        }

        Interval And(const Interval& other) const {
            if (isBottom || other.isBottom) return Interval();
            if (IsConstant() && other.IsConstant()) return Constant(lo & other.lo);
            // Conservative: if either could be zero, result could be zero
            if (Contains(0) || other.Contains(0)) {
                int64_t maxVal = std::max(hi, other.hi);
                return Range(0, maxVal);
            }
            return Range(0, std::max(hi, other.hi));
        }

        Interval Xor(const Interval& other) const {
            if (isBottom || other.isBottom) return Interval();
            if (IsConstant() && other.IsConstant()) return Constant(lo ^ other.lo);
            if (Contains(0) && other.Contains(0)) return Range(0, std::max(hi, other.hi));
            return Top(); // very conservative
        }

        Interval Shl(const Interval& other) const {
            if (isBottom || other.isBottom) return Interval();
            if (IsConstant() && other.IsConstant()) return Constant(lo << other.lo);
            return Range(0, hi << (other.isBottom ? 0 : other.hi));
        }

        Interval Shr(const Interval& other) const {
            if (isBottom || other.isBottom) return Interval();
            if (IsConstant() && other.IsConstant()) return Constant(lo >> other.lo);
            return Range(0, hi);
        }
    };

    enum class PredicateResult {
        Unknown,
        AlwaysTrue,
        AlwaysFalse
    };

    using IntervalDomain = std::map<ZydisRegister, Interval>;

    class AbstractInterpretationPass : public PassRegistrar<AbstractInterpretationPass> {
    public:
        explicit AbstractInterpretationPass()
            : PassRegistrar<AbstractInterpretationPass>()
        { m_type = BaseAnalysisPass::Type::GenericPass; }

        ~AbstractInterpretationPass() = default;

		bool PreRun(AnalysisContext* context) override;
		bool Run(AnalysisContext* context) override;
		bool PostRun(AnalysisContext* context) override;
		std::string_view Name() const override { return "AbstractInterpretationPass"; }
		std::vector<std::string_view> Dependencies() const override { return { "StaticControlFlowRebuilder" }; }

        static PredicateResult EvaluateBranch(ExtendedInstruction* branchInsn, ExtendedInstruction* prevInsn, const IntervalDomain& domain);

    private:
        void AnalyzeFunction(Function* function, AnalysisContext* context);
        IntervalDomain AnalyzeBasicBlock(BasicBlock* bb, AnalysisContext* context, const IntervalDomain& incoming);
        void ApplyInstruction(ExtendedInstruction* insn, IntervalDomain& domain);
        static Interval GetRegisterInterval(const IntervalDomain& domain, ZydisRegister reg);
        void SetRegisterInterval(IntervalDomain& domain, ZydisRegister reg, const Interval& val);
    };
}

#endif
