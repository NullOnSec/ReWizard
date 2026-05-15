#ifndef BASE_PASS_H
#define BASE_PASS_H

#include <string>
#include <string_view>
#include <vector>

namespace ReWizard {
	class AnalysisContext;

	class BaseAnalysisPass {
	public:
		BaseAnalysisPass() {};
		virtual ~BaseAnalysisPass() = default;

		enum class Type {
			GenericPass,
			PEPass,
			MachOPass,
			ELFPass,
		};

		virtual bool PreRun(AnalysisContext* context) = 0;
		virtual bool Run(AnalysisContext* context) = 0;
		virtual bool PostRun(AnalysisContext* context) = 0;
		virtual std::string_view Name() const = 0;
		virtual std::vector<std::string_view> Dependencies() const { return {}; }

		Type GetType() { return m_type; }
		

	protected:
		Type m_type{ Type::GenericPass };
	};

}

#endif
