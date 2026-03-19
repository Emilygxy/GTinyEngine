#pragma once
#include "BaseMaterial.h"
#include <functional>

using FragmentInializer = std::function<void(Fragment&)>;

class IEffectLayersSource
{
public:
    uint8_t GetEffectLayerCount() const noexcept;
	std::shared_ptr<MaterialBase> GetEffectLayer(uint8_t idx) const noexcept;

	bool AddEffectLayer(const std::shared_ptr<MaterialBase>& pMaterial, const FragmentInializer& fragmentInializer = nullptr);
	bool RemoveEffectLayer(uint8_t idx);
	bool RemoveEffectLayer(const std::shared_ptr<MaterialBase>& pMaterial);
	void RemoveEffectLayers();

	void ForEach(const std::function<void(const std::shared_ptr<MaterialBase>&)>& operation);
	void ForEach(const std::function<void(const std::shared_ptr<MaterialBase>&, uint8_t layerIdx)>& operation);

protected:
	virtual void OnAddEffectLayer(const std::shared_ptr<MaterialBase>& pMaterial, uint8_t idx, const FragmentInializer& fragmentInializer = nullptr) = 0;
	virtual void OnRemoveEffectLayer(const std::shared_ptr<MaterialBase>& pMaterial, uint8_t idx) = 0;
	virtual void OnRemoveEffectLayers() = 0;

    private:
	constexpr static const uint8_t kMaxLayerCount = 8u;

    std::vector<std::shared_ptr<MaterialBase>> mEffectLayers;
};