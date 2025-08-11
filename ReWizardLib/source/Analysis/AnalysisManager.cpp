#include <ReWizard/Analysis/AnalysisManager.h>
#include <ReWizard/Analysis/AnalysisContext.h>

#include <spdlog/spdlog.h>

namespace ReWizard {
	AnalysisManager::AnalysisManager(std::unique_ptr<AnalysisContext>& context)
		: m_context(std::move(context)) {
	}

	AnalysisManager::AnalysisManager(const std::unique_ptr<AnalysisContext>& context)
		: m_context(std::move(const_cast<std::unique_ptr<AnalysisContext>&>(context))) {
	}

	std::unique_ptr<AnalysisManager> AnalysisManager::Create(const std::string& target) {
		auto context = AnalysisContext::Create(target);
		if (!context) {
			spdlog::error("Unable to create context!");
			return nullptr;
		}
		return std::unique_ptr<AnalysisManager>(new AnalysisManager(context));
	}

	FileLoader* AnalysisManager::Loader() { return m_context->GetLoader(); }
	const FileLoader* AnalysisManager::Loader() const { return m_context->GetLoader(); }

	std::string AnalysisManager::Name() { return m_context->GetName(); }
	const std::string AnalysisManager::Name() const { return m_context->GetName(); }

}
