//***************************************************************************************
// DeferApp.cpp by Frank Luna (C) 2015 All Rights Reserved.
//***************************************************************************************


#include "../common/d3dApp.h"
#include "../common/MathHelper.h"
#include <UDX12/UploadBuffer.h>
#include "../common/GeometryGenerator.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

const int gNumFrameResources = 3;

struct ObjectConstants
{
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();
};

struct PassConstants
{
	DirectX::XMFLOAT4X4 View = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 InvView = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 Proj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 InvProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 InvViewProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
	float cbPerObjectPad1 = 0.0f;
	DirectX::XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };
	DirectX::XMFLOAT2 InvRenderTargetSize = { 0.0f, 0.0f };
	float NearZ = 0.0f;
	float FarZ = 0.0f;
	float TotalTime = 0.0f;
	float DeltaTime = 0.0f;

	DirectX::XMFLOAT4 AmbientLight = { 0.0f, 0.0f, 0.0f, 1.0f };

	// Indices [0, NUM_DIR_LIGHTS) are directional lights;
	// indices [NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS) are point lights;
	// indices [NUM_DIR_LIGHTS+NUM_POINT_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHT+NUM_SPOT_LIGHTS)
	// are spot lights for a maximum of MaxLights per object.
	Light Lights[MaxLights];
};

struct Vertex
{
	DirectX::XMFLOAT3 Pos;
	DirectX::XMFLOAT3 Normal;
	DirectX::XMFLOAT2 TexC;
};

// Lightweight structure stores parameters to draw a shape.  This will
// vary from app-to-app.
struct RenderItem
{
	RenderItem() = default;

    // World matrix of the shape that describes the object's local space
    // relative to the world space, which defines the position, orientation,
    // and scale of the object in the world.
    XMFLOAT4X4 World = MathHelper::Identity4x4();

	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	// Dirty flag indicating the object data has changed and we need to update the constant buffer.
	// Because we have an object cbuffer for each FrameResource, we have to apply the
	// update to each FrameResource.  Thus, when we modify obect data we should set 
	// NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
	int NumFramesDirty = gNumFrameResources;

	// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
	UINT ObjCBIndex = -1;

	Material* Mat = nullptr;
	Ubpa::DX12::MeshGeometry* Geo = nullptr;
	//std::string Geo;

    // Primitive topology.
    D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    // DrawIndexedInstanced parameters.
    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;
    int BaseVertexLocation = 0;
};

class DeferApp : public D3DApp
{
public:
    DeferApp(HINSTANCE hInstance);
    DeferApp(const DeferApp& rhs) = delete;
    DeferApp& operator=(const DeferApp& rhs) = delete;
    ~DeferApp();

    virtual bool Initialize()override;

private:
    virtual void OnResize()override;
    virtual void Update(const GameTimer& gt)override;
    virtual void Draw(const GameTimer& gt)override;

    virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
    virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
    virtual void OnMouseMove(WPARAM btnState, int x, int y)override;

    void OnKeyboardInput(const GameTimer& gt);
	void UpdateCamera(const GameTimer& gt);
	void AnimateMaterials(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateMaterialCBs(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);

	void LoadTextures();
    void BuildRootSignature();
	void BuildDescriptorHeaps();
    void BuildShadersAndInputLayout();
    void BuildShapeGeometry();
    void BuildPSOs();
    void BuildFrameResources();
    void BuildMaterials();
    void BuildRenderItems();
    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

private:

    //std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	std::vector<std::unique_ptr<Ubpa::DX12::FrameResource>> mFrameResources;
	Ubpa::DX12::FrameResource* mCurrFrameResource = nullptr;
    int mCurrFrameResourceIndex = 0;

    //UINT mCbvSrvDescriptorSize = 0;

    ComPtr<ID3D12RootSignature> mRootSignature = nullptr;

	//ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;
	Ubpa::DX12::DescriptorHeapAllocation mSrvDescriptorHeap;

	//std::unordered_map<std::string, std::unique_ptr<Ubpa::DX12::MeshGeometry>> mGeometries;
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	//std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;

    std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

    //ComPtr<ID3D12PipelineState> mOpaquePSO = nullptr;
 
	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	// Render items divided by PSO.
	std::vector<RenderItem*> mOpaqueRitems;

    PassConstants mMainPassCB;

	XMFLOAT3 mEyePos = { 0.0f, 0.0f, 0.0f };
	XMFLOAT4X4 mView = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();

	float mTheta = 1.3f*XM_PI;
	float mPhi = 0.4f*XM_PI;
	float mRadius = 2.5f;

    POINT mLastMousePos;

	// frame graph
	//Ubpa::DX12::FG::RsrcMngr fgRsrcMngr;
	Ubpa::DX12::FG::Executor fgExecutor;
	Ubpa::FG::Compiler fgCompiler;
	Ubpa::FG::FrameGraph fg;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
    PSTR cmdLine, int showCmd)
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    try
    {
        DeferApp theApp(hInstance);
        if(!theApp.Initialize())
            return 0;

        int rst = theApp.Run();
		Ubpa::DXRenderer::Instance().Release();
		return rst;
    }
    catch(Ubpa::DX12::Util::Exception& e)
    {
        MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
        return 0;
    }

}

DeferApp::DeferApp(HINSTANCE hInstance)
    : D3DApp(hInstance)
{
}

DeferApp::~DeferApp()
{
    if(!uDevice.IsNull())
        FlushCommandQueue();
}

bool DeferApp::Initialize()
{
    if(!D3DApp::Initialize())
        return false;

	Ubpa::DXRenderer::Instance().Init(uDevice.raw.Get());

	Ubpa::DX12::DescriptorHeapMngr::Instance().Init(uDevice.raw.Get(), 1024, 1024, 1024, 1024, 1024);

	//fgRsrcMngr.Init(uGCmdList, uDevice);

    // Reset the command list to prep for initialization commands.
    ThrowIfFailed(uGCmdList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    // Get the increment size of a descriptor in this heap type.  This is hardware specific, 
	// so we have to query this information.
    //mCbvSrvDescriptorSize = uDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	Ubpa::DXRenderer::Instance().GetUpload().Begin();
 
	LoadTextures();
    BuildRootSignature();
	BuildDescriptorHeaps();
    BuildShadersAndInputLayout();
    BuildShapeGeometry();
	BuildMaterials();
    BuildRenderItems();
    BuildFrameResources();
    BuildPSOs();

    // Execute the initialization commands.
    ThrowIfFailed(uGCmdList->Close());
	uCmdQueue.Execute(uGCmdList.raw.Get());

	Ubpa::DXRenderer::Instance().GetUpload().End(uCmdQueue.raw.Get());

    // Wait until initialization is complete.
    FlushCommandQueue();

    return true;
}
 
void DeferApp::OnResize()
{
    D3DApp::OnResize();

    // The window resized, so update the aspect ratio and recompute the projection matrix.
    XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f*MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
    XMStoreFloat4x4(&mProj, P);

	auto clearFGRsrcMngr = [](void* rsrcMngr) {
		reinterpret_cast<Ubpa::DX12::FG::RsrcMngr*>(rsrcMngr)->Clear();
	};
	for (auto& frsrc : mFrameResources)
		frsrc->DelayUpdateResource("FrameGraphRsrcMngr", clearFGRsrcMngr);
}

void DeferApp::Update(const GameTimer& gt)
{
    OnKeyboardInput(gt);
	UpdateCamera(gt);

    // Cycle through the circular frame resource array.
    mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
    mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

    // Has the GPU finished processing the commands of the current frame resource?
    // If not, wait until the GPU has completed commands up to this fence point.
	mCurrFrameResource->Wait();
    /*if(mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
    {
        HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
        ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }*/

	AnimateMaterials(gt);
	UpdateObjectCBs(gt);
	UpdateMaterialCBs(gt);
	UpdateMainPassCB(gt);
}

void DeferApp::Draw(const GameTimer& gt)
{
	auto cmdListAlloc = mCurrFrameResource->GetResource<ID3D12CommandAllocator>("CommandAllocator");

    // Reuse the memory associated with command recording.
    // We can only reset when the associated command lists have finished execution on the GPU.
    ThrowIfFailed(cmdListAlloc->Reset());

    // A command list can be reset after it has been added to the command queue via ExecuteCommandList.
    // Reusing the command list reuses memory.
	ThrowIfFailed(uGCmdList->Reset(cmdListAlloc, Ubpa::DXRenderer::Instance().GetPSO("opaque")));

	uGCmdList.SetDescriptorHeaps(Ubpa::DX12::DescriptorHeapMngr::Instance().GetCSUGpuDH()->GetDescriptorHeap());

	fg.Clear();
	auto fgRsrcMngr = mCurrFrameResource->GetResource<Ubpa::DX12::FG::RsrcMngr>("FrameGraphRsrcMngr");
	fgRsrcMngr->NewFrame();
	fgExecutor.NewFrame();;

	auto renderTexture = fg.AddResourceNode("Render Texture");
	auto backbuffer = fg.AddResourceNode("Back Buffer");
	auto depthstencil = fg.AddResourceNode("Depth Stencil");
	auto rtPass = fg.AddPassNode(
		"Render to Texture",
		{},
		{ renderTexture,depthstencil }
	);
	auto presentPass = fg.AddPassNode(
		"Present",
		{ renderTexture },
		{ backbuffer }
	);

	(*fgRsrcMngr)
		.RegisterTemporalRsrc(renderTexture,
			Ubpa::DX12::FG::RsrcType::RT2D(DXGI_FORMAT_R8G8B8A8_UNORM, mClientWidth, mClientHeight, Colors::LightSteelBlue))
		.RegisterImportedRsrc(backbuffer, { CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT })
		.RegisterImportedRsrc(depthstencil, { mDepthStencilBuffer.Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE })
		.RegisterPassRsrcs(rtPass, renderTexture, D3D12_RESOURCE_STATE_RENDER_TARGET,
			Ubpa::DX12::FG::RsrcImplDesc_RTV_Null{})
		.RegisterPassRsrcs(rtPass, depthstencil,
			D3D12_RESOURCE_STATE_DEPTH_WRITE, Ubpa::DX12::Desc::DSV::Basic(mDepthStencilFormat))
		.RegisterPassRsrcs(presentPass, renderTexture, D3D12_RESOURCE_STATE_COPY_SOURCE,
			Ubpa::DX12::Desc::SRV::Tex2D(CurrentBackBuffer()->GetDesc().Format))
		.RegisterPassRsrcs(presentPass, backbuffer, D3D12_RESOURCE_STATE_COPY_DEST,
			Ubpa::DX12::FG::RsrcImplDesc_RTV_Null{});

	fgExecutor.RegisterPassFunc(
		rtPass,
		[&](const Ubpa::DX12::FG::PassRsrcs& rsrcs) {
			auto rt = rsrcs.find(renderTexture)->second;
			auto ds = rsrcs.find(depthstencil)->second;
			D3D12_VIEWPORT viewport;
			viewport.TopLeftX = 0;
			viewport.TopLeftY = 0;
			viewport.Width = rt.resource->GetDesc().Width;
			viewport.Height = rt.resource->GetDesc().Height;
			viewport.MinDepth = 0.0f;
			viewport.MaxDepth = 1.0f;
			D3D12_RECT rect = { 0, 0, viewport.Width, viewport.Height };
			uGCmdList->RSSetViewports(1, &viewport);
			uGCmdList->RSSetScissorRects(1, &rect);

			// Clear the render texture and depth buffer.
			uGCmdList.ClearRenderTargetView(rt.cpuHandle, Colors::LightSteelBlue);
			uGCmdList.ClearDepthStencilView(ds.cpuHandle);

			// Specify the buffers we are going to render to.
			uGCmdList.OMSetRenderTarget(rt.cpuHandle, ds.cpuHandle);

			uGCmdList->SetGraphicsRootSignature(mRootSignature.Get());

			auto passCB = mCurrFrameResource
				->GetResource<Ubpa::DX12::ArrayUploadBuffer<PassConstants>>("rtPass constants")
				->GetResource();
			uGCmdList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());

			DrawRenderItems(uGCmdList.raw.Get(), mOpaqueRitems);
		}
	);

	fgExecutor.RegisterPassFunc(
		presentPass,
		[&](const Ubpa::DX12::FG::PassRsrcs& rsrcs) {
			auto rt = rsrcs.find(renderTexture)->second;
			auto bb = rsrcs.find(backbuffer)->second;
			uGCmdList->CopyResource(bb.resource, rt.resource);
			//D3D12_VIEWPORT viewport;
			//viewport.TopLeftX = 0;
			//viewport.TopLeftY = 0;
			//viewport.Width = bb.resource->GetDesc().Width;
			//viewport.Height = bb.resource->GetDesc().Height;
			//viewport.MinDepth = 0.0f;
			//viewport.MaxDepth = 1.0f;
			//D3D12_RECT rect = { 0, 0, viewport.Width, viewport.Height };
			//uGCmdList->RSSetViewports(1, &viewport);
			//uGCmdList->RSSetScissorRects(1, &rect);

			//// Clear the render texture and depth buffer.
			//uGCmdList.ClearRenderTargetView(bb.cpuHandle, Colors::LightSteelBlue);

			//// Specify the buffers we are going to render to.
			//uGCmdList.OMSetRenderTarget(bb.cpuHandle, ds.cpuHandle);
			//uGCmdList->OMSetRenderTargets(1, &bb.cpuHandle, false, nullptr);

			//uGCmdList->SetGraphicsRootSignature(mRootSignature.Get());

			//auto passCB = mCurrFrameResource
			//	->GetResource<Ubpa::DX12::ArrayUploadBuffer<PassConstants>>("rtPass constants")
			//	->GetResource();
			//uGCmdList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());

			//DrawRenderItems(uGCmdList.raw.Get(), mOpaqueRitems);

		}
	);

	static bool flag{ false };
	if (!flag) {
		OutputDebugStringA(fg.ToGraphvizGraph().Dump().c_str());
		flag = true;
	}

	auto [success, crst] = fgCompiler.Compile(fg);
	fgExecutor.Execute(crst, *fgRsrcMngr);

    // Done recording commands.
    ThrowIfFailed(uGCmdList->Close());

    // Add the command list to the queue for execution.
	uCmdQueue.Execute(uGCmdList.raw.Get());

    // Swap the back and front buffers
    ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

    //// Advance the fence value to mark commands up to this fence point.
    //mCurrFrameResource->Fence = ++mCurrentFence;

    //// Add an instruction to the command queue to set a new fence point. 
    //// Because we are on the GPU timeline, the new fence point won't be 
    //// set until the GPU finishes processing all the commands prior to this Signal().
    //uCmdQueue->Signal(mFence.Get(), mCurrentFence);
	mCurrFrameResource->Signal(uCmdQueue.raw.Get(), ++mCurrentFence);
}

void DeferApp::OnMouseDown(WPARAM btnState, int x, int y)
{
    mLastMousePos.x = x;
    mLastMousePos.y = y;

    SetCapture(mhMainWnd);
}

void DeferApp::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
}

void DeferApp::OnMouseMove(WPARAM btnState, int x, int y)
{
    if((btnState & MK_LBUTTON) != 0)
    {
        // Make each pixel correspond to a quarter of a degree.
        float dx = XMConvertToRadians(0.25f*static_cast<float>(x - mLastMousePos.x));
        float dy = XMConvertToRadians(0.25f*static_cast<float>(y - mLastMousePos.y));

        // Update angles based on input to orbit camera around box.
        mTheta += dx;
        mPhi += dy;

        // Restrict the angle mPhi.
        mPhi = MathHelper::Clamp(mPhi, 0.1f, MathHelper::Pi - 0.1f);
    }
    else if((btnState & MK_RBUTTON) != 0)
    {
        // Make each pixel correspond to 0.2 unit in the scene.
        float dx = 0.05f*static_cast<float>(x - mLastMousePos.x);
        float dy = 0.05f*static_cast<float>(y - mLastMousePos.y);

        // Update the camera radius based on input.
        mRadius += dx - dy;

        // Restrict the radius.
        mRadius = MathHelper::Clamp(mRadius, 5.0f, 150.0f);
    }

    mLastMousePos.x = x;
    mLastMousePos.y = y;
}
 
void DeferApp::OnKeyboardInput(const GameTimer& gt)
{
}
 
void DeferApp::UpdateCamera(const GameTimer& gt)
{
	// Convert Spherical to Cartesian coordinates.
	mEyePos.x = mRadius*sinf(mPhi)*cosf(mTheta);
	mEyePos.z = mRadius*sinf(mPhi)*sinf(mTheta);
	mEyePos.y = mRadius*cosf(mPhi);

	// Build the view matrix.
	XMVECTOR pos = XMVectorSet(mEyePos.x, mEyePos.y, mEyePos.z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&mView, view);
}

void DeferApp::AnimateMaterials(const GameTimer& gt)
{
	
}

void DeferApp::UpdateObjectCBs(const GameTimer& gt)
{
	auto currObjectCB = mCurrFrameResource
		->GetResource<Ubpa::DX12::ArrayUploadBuffer<ObjectConstants>>("ArrayUploadBuffer<ObjectConstants>");
	for(auto& e : mAllRitems)
	{
		// Only update the cbuffer data if the constants have changed.  
		// This needs to be tracked per frame resource.
		if(e->NumFramesDirty > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&e->World);
			XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));

			currObjectCB->Set(e->ObjCBIndex, objConstants);

			// Next FrameResource need to be updated too.
			e->NumFramesDirty--;
		}
	}
}

void DeferApp::UpdateMaterialCBs(const GameTimer& gt)
{
	auto currMaterialCB = mCurrFrameResource
		->GetResource<Ubpa::DX12::ArrayUploadBuffer<MaterialConstants>>("ArrayUploadBuffer<MaterialConstants>");
	for(auto& e : mMaterials)
	{
		// Only update the cbuffer data if the constants have changed.  If the cbuffer
		// data changes, it needs to be updated for each FrameResource.
		Material* mat = e.second.get();
		if(mat->NumFramesDirty > 0)
		{
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

			MaterialConstants matConstants;
			matConstants.DiffuseAlbedo = mat->DiffuseAlbedo;
			matConstants.FresnelR0 = mat->FresnelR0;
			matConstants.Roughness = mat->Roughness;
			XMStoreFloat4x4(&matConstants.MatTransform, XMMatrixTranspose(matTransform));

			currMaterialCB->Set(mat->MatCBIndex, matConstants);

			// Next FrameResource need to be updated too.
			mat->NumFramesDirty--;
		}
	}
}

void DeferApp::UpdateMainPassCB(const GameTimer& gt)
{
	XMMATRIX view = XMLoadFloat4x4(&mView);
	XMMATRIX proj = XMLoadFloat4x4(&mProj);

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	mMainPassCB.EyePosW = mEyePos;
	mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mMainPassCB.NearZ = 1.0f;
	mMainPassCB.FarZ = 1000.0f;
	mMainPassCB.TotalTime = gt.TotalTime();
	mMainPassCB.DeltaTime = gt.DeltaTime();
	mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
	mMainPassCB.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[0].Strength = { 0.6f, 0.6f, 0.6f };
	mMainPassCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[1].Strength = { 0.3f, 0.3f, 0.3f };
	mMainPassCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
	mMainPassCB.Lights[2].Strength = { 0.15f, 0.15f, 0.15f };

	auto currPassCB = mCurrFrameResource
		->GetResource<Ubpa::DX12::ArrayUploadBuffer<PassConstants>>("rtPass constants");
	currPassCB->Set(0, mMainPassCB);
}

void DeferApp::LoadTextures()
{
	/*auto woodCrateTex = std::make_unique<Texture>();
	woodCrateTex->Name = "woodCrateTex";
	woodCrateTex->Filename = L"../data/textures/WoodCrate01.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(uDevice.raw.Get(),
		uGCmdList.raw.Get(), woodCrateTex->Filename.c_str(),
		woodCrateTex->Resource, woodCrateTex->UploadHeap));
 
	mTextures[woodCrateTex->Name] = std::move(woodCrateTex);*/
	Ubpa::DXRenderer::Instance().RegisterDDSTextureFromFile(
		Ubpa::DXRenderer::Instance().GetUpload(),
		"woodCrateTex",
		L"../data/textures/WoodCrate01.dds");
}

void DeferApp::BuildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE texTable;
	texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

    // Root parameter can be a table, root descriptor or root constants.
    CD3DX12_ROOT_PARAMETER slotRootParameter[4];

	// Perfomance TIP: Order from most frequent to least frequent.
	slotRootParameter[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
	//slotRootParameter[0].InitAsShaderResourceView(0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
    slotRootParameter[1].InitAsConstantBufferView(0);
    slotRootParameter[2].InitAsConstantBufferView(1);
    slotRootParameter[3].InitAsConstantBufferView(2);

	auto staticSamplers = GetStaticSamplers();

    // A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter,
		(UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    // create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
        serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

    if(errorBlob != nullptr)
    {
        ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }
    ThrowIfFailed(hr);

    ThrowIfFailed(uDevice->CreateRootSignature(
		0,
        serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

void DeferApp::BuildDescriptorHeaps()
{
	//
	// Create the SRV heap.
	//
	/*D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = 1;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(uDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));*/

	//mSrvDescriptorHeap = Ubpa::DX12::DescriptorHeapMngr::Instance().GetCSUGpuDH()->Allocate(1);

	////
	//// Fill out the heap with actual descriptors.
	////
	///*CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());*/
	//CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap.GetCpuHandle());

	//auto woodCrateTex = mTextures["woodCrateTex"]->Resource;
 //
	//uDevice.CreateSRV_Tex2D(woodCrateTex.Get(), hDescriptor);
}

void DeferApp::BuildShadersAndInputLayout()
{
	//mShaders["standardVS"] = Ubpa::DX12::Util::CompileShader(L"..\\data\\shaders\\00_crate\\Default.hlsl", nullptr, "VS", "vs_5_0");
	//mShaders["opaquePS"] = Ubpa::DX12::Util::CompileShader(L"..\\data\\shaders\\00_crate\\Default.hlsl", nullptr, "PS", "ps_5_0");
	Ubpa::DXRenderer::Instance().RegisterShaderByteCode("standardVS",
		L"..\\data\\shaders\\01_defer\\Default.hlsl", nullptr, "VS", "vs_5_0");
	Ubpa::DXRenderer::Instance().RegisterShaderByteCode("opaquePS",
		L"..\\data\\shaders\\01_defer\\Default.hlsl", nullptr, "PS", "ps_5_0");
	
    mInputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
}

void DeferApp::BuildShapeGeometry()
{
    GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 3);
 
	Ubpa::DX12::SubmeshGeometry boxSubmesh;
	boxSubmesh.IndexCount = (UINT)box.Indices32.size();
	boxSubmesh.StartIndexLocation = 0;
	boxSubmesh.BaseVertexLocation = 0;

 
	std::vector<Vertex> vertices(box.Vertices.size());

	for(size_t i = 0; i < box.Vertices.size(); ++i)
	{
		vertices[i].Pos = box.Vertices[i].Position;
		vertices[i].Normal = box.Vertices[i].Normal;
		vertices[i].TexC = box.Vertices[i].TexC;
	}

	std::vector<std::uint16_t> indices = box.GetIndices16();

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices.size()  * sizeof(std::uint16_t);

	/*auto geo = std::make_unique<Ubpa::DX12::MeshGeometry>();
	geo->Name = "boxGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = Ubpa::DX12::Util::CreateDefaultBuffer(uDevice.raw.Get(),
		uGCmdList.raw.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = Ubpa::DX12::Util::CreateDefaultBuffer(uDevice.raw.Get(),
		uGCmdList.raw.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;*/

	/*geo->InitBuffer(uDevice.raw.Get(), Ubpa::DXRenderer::Instance().GetUpload(),
		vertices.data(), (UINT)vertices.size(), sizeof(Vertex),
		indices.data(), (UINT)indices.size(), DXGI_FORMAT_R16_UINT);

	geo->submeshGeometries["box"] = boxSubmesh;

	mGeometries[geo->Name] = std::move(geo);*/

	Ubpa::DXRenderer::Instance()
		.RegisterStaticMeshGeometry(
			Ubpa::DXRenderer::Instance().GetUpload(), "boxGeo",
			vertices.data(), (UINT)vertices.size(), sizeof(Vertex),
			indices.data(), (UINT)indices.size(), DXGI_FORMAT_R16_UINT)
		.submeshGeometries["box"] = boxSubmesh;
}

void DeferApp::BuildPSOs()
{
 //   D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;

	////
	//// PSO for opaque objects.
	////
 //   ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	//opaquePsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	//opaquePsoDesc.pRootSignature = mRootSignature.Get();
	//opaquePsoDesc.VS = 
	//{ 
	//	reinterpret_cast<BYTE*>(Ubpa::DXRenderer::Instance().GetShaderByteCode("standardVS")->GetBufferPointer()),
	//	Ubpa::DXRenderer::Instance().GetShaderByteCode("standardVS")->GetBufferSize()
	//};
	//opaquePsoDesc.PS = 
	//{ 
	//	reinterpret_cast<BYTE*>(Ubpa::DXRenderer::Instance().GetShaderByteCode("opaquePS")->GetBufferPointer()),
	//	Ubpa::DXRenderer::Instance().GetShaderByteCode("opaquePS")->GetBufferSize()
	//};
	//opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	//opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	//opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	//opaquePsoDesc.SampleMask = UINT_MAX;
	//opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	//opaquePsoDesc.NumRenderTargets = 1;
	//opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;
	//opaquePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	//opaquePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	//opaquePsoDesc.DSVFormat = mDepthStencilFormat;
	auto opaquePsoDesc = Ubpa::DX12::Desc::PSO::Basic(
		mRootSignature.Get(),
		mInputLayout.data(), (UINT)mInputLayout.size(),
		Ubpa::DXRenderer::Instance().GetShaderByteCode("standardVS"),
		Ubpa::DXRenderer::Instance().GetShaderByteCode("opaquePS"),
		mBackBufferFormat,
		mDepthStencilFormat
	);
    //ThrowIfFailed(uDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mOpaquePSO)));
	Ubpa::DXRenderer::Instance().RegisterPSO("opaque", &opaquePsoDesc);
}

void DeferApp::BuildFrameResources()
{
    for(int i = 0; i < gNumFrameResources; ++i)
    {
		auto fr = std::make_unique<Ubpa::DX12::FrameResource>(mFence.Get());

		ID3D12CommandAllocator* allocator;
		ThrowIfFailed(uDevice->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(&allocator)));

		fr->RegisterResource("CommandAllocator", allocator, [](void* allocator) {
			reinterpret_cast<ID3D12CommandAllocator*>(allocator)->Release();
		});

		fr->RegisterResource("rtPass constants",
			new Ubpa::DX12::ArrayUploadBuffer<PassConstants>{ uDevice.raw.Get(), 1, true });

		fr->RegisterResource("ArrayUploadBuffer<MaterialConstants>",
			new Ubpa::DX12::ArrayUploadBuffer<MaterialConstants>{ uDevice.raw.Get(), mMaterials.size(), true });

		fr->RegisterResource("ArrayUploadBuffer<ObjectConstants>",
			new Ubpa::DX12::ArrayUploadBuffer<ObjectConstants>{ uDevice.raw.Get(), mAllRitems.size(), true });

		auto fgRsrcMngr = new Ubpa::DX12::FG::RsrcMngr;
		fgRsrcMngr->Init(uGCmdList, uDevice);
		fr->RegisterResource("FrameGraphRsrcMngr", fgRsrcMngr);

		mFrameResources.emplace_back(std::move(fr));

		// We cannot update a cbuffer until the GPU is done processing the commands
		// that reference it.  So each frame needs their own cbuffers.
		// std::unique_ptr<Ubpa::DX12::ArrayUploadBuffer<FrameConstants>> FrameCB = nullptr;
		/*std::unique_ptr<Ubpa::DX12::ArrayUploadBuffer<PassConstants>> PassCB = nullptr;
		std::unique_ptr<Ubpa::DX12::ArrayUploadBuffer<MaterialConstants>> MaterialCB = nullptr;
		std::unique_ptr<Ubpa::DX12::ArrayUploadBuffer<ObjectConstants>> ObjectCB = nullptr;

        mFrameResources.push_back(std::make_unique<FrameResource>(uDevice.raw.Get(),
            1, (UINT)mAllRitems.size(), (UINT)mMaterials.size()));*/
    }
}

void DeferApp::BuildMaterials()
{
	auto woodCrate = std::make_unique<Material>();
	woodCrate->Name = "woodCrate";
	woodCrate->MatCBIndex = 0;
	woodCrate->DiffuseSrvGpuHandle = Ubpa::DXRenderer::Instance().GetTextureSrvGpuHandle("woodCrateTex");
	woodCrate->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	woodCrate->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
	woodCrate->Roughness = 0.2f;

	mMaterials["woodCrate"] = std::move(woodCrate);
}

void DeferApp::BuildRenderItems()
{
	auto boxRitem = std::make_unique<RenderItem>();
	boxRitem->ObjCBIndex = 0;
	boxRitem->Mat = mMaterials["woodCrate"].get();
	boxRitem->Geo = &Ubpa::DXRenderer::Instance().GetMeshGeometry("boxGeo");
	boxRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem->IndexCount = boxRitem->Geo->submeshGeometries["box"].IndexCount;
	boxRitem->StartIndexLocation = boxRitem->Geo->submeshGeometries["box"].StartIndexLocation;
	boxRitem->BaseVertexLocation = boxRitem->Geo->submeshGeometries["box"].BaseVertexLocation;
	mAllRitems.push_back(std::move(boxRitem));

	// All the render items are opaque.
	for(auto& e : mAllRitems)
		mOpaqueRitems.push_back(e.get());
}

void DeferApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
    UINT objCBByteSize = Ubpa::DX12::Util::CalcConstantBufferByteSize(sizeof(ObjectConstants));
    UINT matCBByteSize = Ubpa::DX12::Util::CalcConstantBufferByteSize(sizeof(MaterialConstants));
 
	auto objectCB = mCurrFrameResource
		->GetResource<Ubpa::DX12::ArrayUploadBuffer<ObjectConstants>>("ArrayUploadBuffer<ObjectConstants>")
		->GetResource();
	auto matCB = mCurrFrameResource
		->GetResource<Ubpa::DX12::ArrayUploadBuffer<MaterialConstants>>("ArrayUploadBuffer<MaterialConstants>")
		->GetResource();

    // For each render item...
    for(size_t i = 0; i < ritems.size(); ++i)
    {
        auto ri = ritems[i];

        cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
        cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
        cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

		/*CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());*/
		/*CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mSrvDescriptorHeap.GetGpuHandle());
		tex.Offset(ri->Mat->DiffuseSrvGpuHandle, mCbvSrvDescriptorSize);*/

        D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex*objCBByteSize;
		D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + ri->Mat->MatCBIndex*matCBByteSize;

		cmdList->SetGraphicsRootDescriptorTable(0, ri->Mat->DiffuseSrvGpuHandle);
		//cmdList->SetGraphicsRootShaderResourceView(0, mTextures["woodCrate"]->Resource->GetGPUVirtualAddress());
        cmdList->SetGraphicsRootConstantBufferView(1, objCBAddress);
        cmdList->SetGraphicsRootConstantBufferView(3, matCBAddress);

        cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
    }
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> DeferApp::GetStaticSamplers()
{
	// Applications usually only need a handful of samplers.  So just define them all up front
	// and keep them available as part of the root signature.  

	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	return { 
		pointWrap, pointClamp,
		linearWrap, linearClamp, 
		anisotropicWrap, anisotropicClamp };
}

