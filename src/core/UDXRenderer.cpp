#include <UDXRenderer/UDXRenderer.h>

#include <UDX12/UDX12.h>

#include <UDX12/_deps/DirectXTK12/ResourceUploadBatch.h>
#include <UDX12/_deps/DirectXTK12/DDSTextureLoader.h>

#include <unordered_map>
#include <iostream>

using namespace Ubpa;

struct DXRenderer::Impl {
    struct Texture {
        DX12::DescriptorHeapAllocation allocation;
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;
        ID3D12Resource* resource{ nullptr };
    };

    bool isInit{ false };
    ID3D12Device* device{ nullptr };
    DirectX::ResourceUploadBatch* upload;
    std::unordered_map<std::string, Texture> textureMap;
};

DXRenderer::DXRenderer()
	: pImpl(new Impl)
{
}

DXRenderer::~DXRenderer() {
    assert(!pImpl->isInit);
    delete(pImpl);
}

DXRenderer& DXRenderer::Init(ID3D12Device* device) {
    assert(!pImpl->isInit);

    pImpl->device = device;
    pImpl->upload = new DirectX::ResourceUploadBatch{ device };
    
    pImpl->isInit = true;
    return *this;
}

void DXRenderer::Release() {
    assert(pImpl->isInit);

    pImpl->device = nullptr;
    delete pImpl->upload;

    for (auto& [name, tex] : pImpl->textureMap) {
        DX12::DescriptorHeapMngr::Instance().GetCSUGpuDH()->Free(std::move(tex.allocation));
        tex.resource->Release();
    }
    pImpl->textureMap.clear();

    pImpl->isInit = false;
}

DirectX::ResourceUploadBatch& DXRenderer::GetUpload() const {
    return *pImpl->upload;
}

DXRenderer& DXRenderer::RegisterDDSTextureFromFile(
    DirectX::ResourceUploadBatch& upload,
    std::string name,
    std::wstring filename)
{
    Impl::Texture& tex = pImpl->textureMap[name];

    bool isCubeMap;
    DirectX::CreateDDSTextureFromFile(
        pImpl->device,
        upload,
        filename.data(),
        &tex.resource,
        false,
        0,
        nullptr,
        &isCubeMap);

    if (!isCubeMap) {
        tex.allocation = DX12::DescriptorHeapMngr::Instance().GetCSUGpuDH()->Allocate(1);
        tex.cpuHandle = tex.allocation.GetCpuHandle(0);
        tex.gpuHandle = tex.allocation.GetGpuHandle(0);
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = DX12::Desc::SRC::Tex2D(tex.resource);
        pImpl->device->CreateShaderResourceView(tex.resource, &srvDesc, tex.cpuHandle);
    }
    else
        assert("not support");

	return *this;
}

D3D12_GPU_DESCRIPTOR_HANDLE DXRenderer::GetTextureGpuHandle(const std::string& name) const {
    return pImpl->textureMap.find(name)->second.gpuHandle;
}