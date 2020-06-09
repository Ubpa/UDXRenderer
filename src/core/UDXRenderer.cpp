#include <UDXRenderer/UDXRenderer.h>

#include <unordered_map>
#include <iostream>

using namespace Ubpa;

struct DXRenderer::Impl {
    struct Texture {
        ID3D12Resource* resource{ nullptr };
        DX12::DescriptorHeapAllocation allocationSRV;
        DX12::DescriptorHeapAllocation allocationRTV;
    };

    bool isInit{ false };
    ID3D12Device* device{ nullptr };
    DirectX::ResourceUploadBatch* upload;

    std::unordered_map<std::string, Texture> textureMap;

    std::unordered_map<std::string, DX12::MeshGeometry> meshGeoMap;
    std::unordered_map<std::string, ID3DBlob*> shaderByteCodeMap;
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

    for (auto& [name, tex] : pImpl->textureMap) {
        DX12::DescriptorHeapMngr::Instance().GetCSUGpuDH()->Free(std::move(tex.allocationSRV));
        if(!tex.allocationRTV.IsNull())
            DX12::DescriptorHeapMngr::Instance().GetRTVCpuDH()->Free(std::move(tex.allocationRTV));
        tex.resource->Release();
    }

    pImpl->device = nullptr;
    delete pImpl->upload;

    pImpl->textureMap.clear();
    pImpl->meshGeoMap.clear();

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
    Impl::Texture tex;

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

    tex.allocationSRV = DX12::DescriptorHeapMngr::Instance().GetCSUGpuDH()->Allocate(1);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc =
        isCubeMap ?
        DX12::Desc::SRV::TexCube(tex.resource)
        : DX12::Desc::SRV::Tex2D(tex.resource);

    pImpl->device->CreateShaderResourceView(tex.resource, &srvDesc, tex.allocationSRV.GetCpuHandle());

    pImpl->textureMap.emplace(std::move(name), std::move(tex));

	return *this;
}

D3D12_CPU_DESCRIPTOR_HANDLE DXRenderer::GetTextureSrvCpuHandle(const std::string& name) const {
    return pImpl->textureMap.find(name)->second.allocationSRV.GetCpuHandle();
}
D3D12_GPU_DESCRIPTOR_HANDLE DXRenderer::GetTextureSrvGpuHandle(const std::string& name) const {
    return pImpl->textureMap.find(name)->second.allocationSRV.GetGpuHandle();
}

DX12::DescriptorHeapAllocation& DXRenderer::GetTextureRtvs(const std::string& name) const {
    return pImpl->textureMap.find(name)->second.allocationRTV;
}

DX12::MeshGeometry& DXRenderer::RegisterStaticMeshGeometry(
    DirectX::ResourceUploadBatch& upload, std::string name,
    const void* vb_data, UINT vb_count, UINT vb_stride,
    const void* ib_data, UINT ib_count, DXGI_FORMAT ib_format)
{
    auto& meshGeo = pImpl->meshGeoMap[name];
    meshGeo.Name = std::move(name);
    meshGeo.InitBuffer(pImpl->device, upload, vb_data, vb_count, vb_stride, ib_data, ib_count, ib_format);
    return meshGeo;
}

DX12::MeshGeometry& DXRenderer::RegisterDynamicMeshGeometry(
    std::string name,
    const void* vb_data, UINT vb_count, UINT vb_stride,
    const void* ib_data, UINT ib_count, DXGI_FORMAT ib_format)
{
    auto& meshGeo = pImpl->meshGeoMap[name];
    meshGeo.Name = std::move(name);
    meshGeo.InitBuffer(pImpl->device, vb_data, vb_count, vb_stride, ib_data, ib_count, ib_format);
    return meshGeo;
}

DX12::MeshGeometry& DXRenderer::GetMeshGeometry(const std::string& name) const {
    return pImpl->meshGeoMap.find(name)->second;
}

ID3DBlob* DXRenderer::RegisterShaderByteCode(
    std::string name,
    const std::wstring& filename,
    const D3D_SHADER_MACRO* defines,
    const std::string& entrypoint,
    const std::string& target)
{
    auto shader = DX12::Util::CompileShader(filename, defines, entrypoint, target);
    pImpl->shaderByteCodeMap.emplace(std::move(name), shader);
    return shader;
}

ID3DBlob* DXRenderer::GetShaderByteCode(const std::string& name) const {
    return pImpl->shaderByteCodeMap.find(name)->second;
}

DXRenderer& DXRenderer::RegisterRenderTexture2D(std::string name, UINT width, UINT height, DXGI_FORMAT format) {
    Impl::Texture tex;

    tex.allocationSRV = DX12::DescriptorHeapMngr::Instance().GetCSUGpuDH()->Allocate(1);
    tex.allocationRTV = DX12::DescriptorHeapMngr::Instance().GetRTVCpuDH()->Allocate(1);

    // create resource
    D3D12_RESOURCE_DESC texDesc;
    ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.MipLevels = 1;
    texDesc.Format = format;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    ThrowIfFailed(pImpl->device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE, &texDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&tex.resource)));

    // create SRV
    pImpl->device->CreateShaderResourceView(
        tex.resource,
        &DX12::Desc::SRV::Tex2D(tex.resource),
        tex.allocationSRV.GetCpuHandle());

    // create RTVs
    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
    ZeroMemory(&rtvDesc, sizeof(rtvDesc));
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
    rtvDesc.Format = format;
    rtvDesc.Texture2D.MipSlice = 0;
    rtvDesc.Texture2D.PlaneSlice = 0; // ?
    pImpl->device->CreateRenderTargetView(tex.resource, &rtvDesc, tex.allocationRTV.GetCpuHandle());

    pImpl->textureMap.emplace(std::move(name), std::move(tex));

    return *this;
}

DXRenderer& DXRenderer::RegisterRenderTextureCube(std::string name, UINT size, DXGI_FORMAT format) {
    Impl::Texture tex;

    tex.allocationSRV = DX12::DescriptorHeapMngr::Instance().GetCSUGpuDH()->Allocate(1);
    tex.allocationRTV = DX12::DescriptorHeapMngr::Instance().GetRTVCpuDH()->Allocate(6);

    // create resource
    D3D12_RESOURCE_DESC texDesc;
    ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Alignment = 0;
    texDesc.Width = size;
    texDesc.Height = size;
    texDesc.DepthOrArraySize = 6;
    texDesc.MipLevels = 1;
    texDesc.Format = format;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    ThrowIfFailed(pImpl->device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE, &texDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&tex.resource)));

    // create SRV
    pImpl->device->CreateShaderResourceView(
        tex.resource,
        &DX12::Desc::SRV::TexCube(tex.resource),
        tex.allocationSRV.GetCpuHandle());

    // create RTVs
    for (UINT i = 0; i < 6; i++)
    {
        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
        ZeroMemory(&rtvDesc, sizeof(rtvDesc));
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
        rtvDesc.Format = format;
        rtvDesc.Texture2DArray.MipSlice = 0;
        rtvDesc.Texture2DArray.PlaneSlice = 0;
        rtvDesc.Texture2DArray.FirstArraySlice = i;
        rtvDesc.Texture2DArray.ArraySize = 1;
        pImpl->device->CreateRenderTargetView(tex.resource, &rtvDesc, tex.allocationRTV.GetCpuHandle(i));
    }

    pImpl->textureMap.emplace(std::move(name), std::move(tex));

    return *this;
}
