#include <UDXRenderer/UDXRenderer.h>

#include <unordered_map>
#include <iostream>

using namespace Ubpa;

struct DXRenderer::Impl {
    struct Texture {
        std::vector<ID3D12Resource*> resources;
        UDX12::DescriptorHeapAllocation allocationSRV;
        UDX12::DescriptorHeapAllocation allocationRTV;
    };

    bool isInit{ false };
    ID3D12Device* device{ nullptr };
    DirectX::ResourceUploadBatch* upload;

    std::unordered_map<std::string, Texture> textureMap;

    std::unordered_map<std::string, UDX12::MeshGeometry> meshGeoMap;
    std::unordered_map<std::string, ID3DBlob*> shaderByteCodeMap;
    std::unordered_map<std::string, ID3D12RootSignature*> rootSignatureMap;
    std::unordered_map<std::string, ID3D12PipelineState*> PSOMap;
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
        UDX12::DescriptorHeapMngr::Instance().GetCSUGpuDH()->Free(std::move(tex.allocationSRV));
        if(!tex.allocationRTV.IsNull())
            UDX12::DescriptorHeapMngr::Instance().GetRTVCpuDH()->Free(std::move(tex.allocationRTV));
        for (auto rsrc : tex.resources)
            rsrc->Release();
    }

    for (auto& [name, rootSig] : pImpl->rootSignatureMap)
        rootSig->Release();

    for (auto& [name, PSO] : pImpl->PSOMap)
        PSO->Release();

    pImpl->device = nullptr;
    delete pImpl->upload;

    pImpl->textureMap.clear();
    pImpl->meshGeoMap.clear();
    pImpl->rootSignatureMap.clear();
    pImpl->PSOMap.clear();

    pImpl->isInit = false;
}

DirectX::ResourceUploadBatch& DXRenderer::GetUpload() const {
    return *pImpl->upload;
}

DXRenderer& DXRenderer::RegisterDDSTextureFromFile(
    DirectX::ResourceUploadBatch& upload,
    std::string name,
    std::wstring_view filename)
{
    return RegisterDDSTextureArrayFromFile(upload, name, &filename, 1);
}

DXRenderer& DXRenderer::RegisterDDSTextureArrayFromFile(DirectX::ResourceUploadBatch& upload,
    std::string name, const std::wstring_view* filenameArr, UINT num)
{
    Impl::Texture tex;
    tex.resources.resize(num);

    tex.allocationSRV = UDX12::DescriptorHeapMngr::Instance().GetCSUGpuDH()->Allocate(num);

    for (UINT i = 0; i < num; i++) {
        bool isCubeMap;
        DirectX::CreateDDSTextureFromFile(
            pImpl->device,
            upload,
            filenameArr[i].data(),
            &tex.resources[i],
            false,
            0,
            nullptr,
            &isCubeMap);

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc =
            isCubeMap ?
            UDX12::Desc::SRV::TexCube(tex.resources[i]->GetDesc().Format)
            : UDX12::Desc::SRV::Tex2D(tex.resources[i]->GetDesc().Format);

        pImpl->device->CreateShaderResourceView(tex.resources[i], &srvDesc, tex.allocationSRV.GetCpuHandle(i));
    }

    pImpl->textureMap.emplace(std::move(name), std::move(tex));

    return *this;
}

D3D12_CPU_DESCRIPTOR_HANDLE DXRenderer::GetTextureSrvCpuHandle(const std::string& name, UINT index) const {
    return pImpl->textureMap.find(name)->second.allocationSRV.GetCpuHandle(index);
}
D3D12_GPU_DESCRIPTOR_HANDLE DXRenderer::GetTextureSrvGpuHandle(const std::string& name, UINT index) const {
    return pImpl->textureMap.find(name)->second.allocationSRV.GetGpuHandle(index);
}

UDX12::DescriptorHeapAllocation& DXRenderer::GetTextureRtvs(const std::string& name) const {
    return pImpl->textureMap.find(name)->second.allocationRTV;
}

UDX12::MeshGeometry& DXRenderer::RegisterStaticMeshGeometry(
    DirectX::ResourceUploadBatch& upload, std::string name,
    const void* vb_data, UINT vb_count, UINT vb_stride,
    const void* ib_data, UINT ib_count, DXGI_FORMAT ib_format)
{
    auto& meshGeo = pImpl->meshGeoMap[name];
    meshGeo.Name = std::move(name);
    meshGeo.InitBuffer(pImpl->device, upload, vb_data, vb_count, vb_stride, ib_data, ib_count, ib_format);
    return meshGeo;
}

UDX12::MeshGeometry& DXRenderer::RegisterDynamicMeshGeometry(
    std::string name,
    const void* vb_data, UINT vb_count, UINT vb_stride,
    const void* ib_data, UINT ib_count, DXGI_FORMAT ib_format)
{
    auto& meshGeo = pImpl->meshGeoMap[name];
    meshGeo.Name = std::move(name);
    meshGeo.InitBuffer(pImpl->device, vb_data, vb_count, vb_stride, ib_data, ib_count, ib_format);
    return meshGeo;
}

UDX12::MeshGeometry& DXRenderer::GetMeshGeometry(const std::string& name) const {
    return pImpl->meshGeoMap.find(name)->second;
}

ID3DBlob* DXRenderer::RegisterShaderByteCode(
    std::string name,
    const std::wstring& filename,
    const D3D_SHADER_MACRO* defines,
    const std::string& entrypoint,
    const std::string& target)
{
    auto shader = UDX12::Util::CompileShader(filename, defines, entrypoint, target);
    pImpl->shaderByteCodeMap.emplace(std::move(name), shader);
    return shader;
}

ID3DBlob* DXRenderer::GetShaderByteCode(const std::string& name) const {
    return pImpl->shaderByteCodeMap.find(name)->second;
}

DXRenderer& DXRenderer::RegisterRenderTexture2D(std::string name, UINT width, UINT height, DXGI_FORMAT format) {
    Impl::Texture tex;
    tex.resources.resize(1);

    tex.allocationSRV = UDX12::DescriptorHeapMngr::Instance().GetCSUGpuDH()->Allocate(1);
    tex.allocationRTV = UDX12::DescriptorHeapMngr::Instance().GetRTVCpuDH()->Allocate(1);

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
        IID_PPV_ARGS(&tex.resources[0])));

    // create SRV
    pImpl->device->CreateShaderResourceView(
        tex.resources[0],
        &UDX12::Desc::SRV::Tex2D(format),
        tex.allocationSRV.GetCpuHandle());

    // create RTVs
    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
    ZeroMemory(&rtvDesc, sizeof(rtvDesc));
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
    rtvDesc.Format = format;
    rtvDesc.Texture2D.MipSlice = 0;
    rtvDesc.Texture2D.PlaneSlice = 0; // ?
    pImpl->device->CreateRenderTargetView(tex.resources[0], &rtvDesc, tex.allocationRTV.GetCpuHandle());

    pImpl->textureMap.emplace(std::move(name), std::move(tex));

    return *this;
}

DXRenderer& DXRenderer::RegisterRenderTextureCube(std::string name, UINT size, DXGI_FORMAT format) {
    Impl::Texture tex;
    tex.resources.resize(1);

    tex.allocationSRV = UDX12::DescriptorHeapMngr::Instance().GetCSUGpuDH()->Allocate(1);
    tex.allocationRTV = UDX12::DescriptorHeapMngr::Instance().GetRTVCpuDH()->Allocate(6);

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
        IID_PPV_ARGS(&tex.resources[0])));

    // create SRV
    pImpl->device->CreateShaderResourceView(
        tex.resources[0],
        &UDX12::Desc::SRV::TexCube(format),
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
        pImpl->device->CreateRenderTargetView(tex.resources[0], &rtvDesc, tex.allocationRTV.GetCpuHandle(i));
    }

    pImpl->textureMap.emplace(std::move(name), std::move(tex));

    return *this;
}

DXRenderer& DXRenderer::RegisterRootSignature(
    std::string name,
    const D3D12_ROOT_SIGNATURE_DESC* desc
)
{
    // create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
    ID3DBlob* serializedRootSig = nullptr;
    ID3DBlob* errorBlob = nullptr;

    HRESULT hr = D3D12SerializeRootSignature(desc, D3D_ROOT_SIGNATURE_VERSION_1,
        &serializedRootSig, &errorBlob);

    if (errorBlob != nullptr)
    {
        ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        errorBlob->Release();
    }
    ThrowIfFailed(hr);

    ID3D12RootSignature* rootSig;

    ThrowIfFailed(pImpl->device->CreateRootSignature(
        0,
        serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(&rootSig)));

    pImpl->rootSignatureMap.emplace(std::move(name), rootSig);

    serializedRootSig->Release();

    return *this;
}

ID3D12RootSignature* DXRenderer::GetRootSignature(const std::string& name) const {
    return pImpl->rootSignatureMap.find(name)->second;
}

DXRenderer& DXRenderer::RegisterPSO(
    std::string name,
    const D3D12_GRAPHICS_PIPELINE_STATE_DESC* desc)
{
    ID3D12PipelineState* pso;
    pImpl->device->CreateGraphicsPipelineState(desc, IID_PPV_ARGS(&pso));
    pImpl->PSOMap.emplace(std::move(name), pso);
    return *this;
}

ID3D12PipelineState* DXRenderer::GetPSO(const std::string& name) const {
    return pImpl->PSOMap.find(name)->second;
}
