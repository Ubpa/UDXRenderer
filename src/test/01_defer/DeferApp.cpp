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
	Ubpa::UDX12::MeshGeometry* Geo = nullptr;
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

private:

	std::vector<std::unique_ptr<Ubpa::UDX12::FrameRsrcMngr>> mFrameResources;
	Ubpa::UDX12::FrameRsrcMngr* mCurrFrameRsrcMngr = nullptr;
    int mCurrFrameRsrcMngrIndex = 0;

	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;

    std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
 
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
	//Ubpa::UDX12::FG::RsrcMngr fgRsrcMngr;
	Ubpa::UDX12::FG::Executor fgExecutor;
	Ubpa::UFG::Compiler fgCompiler;
	Ubpa::UFG::FrameGraph fg;
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
    catch(Ubpa::UDX12::Util::Exception& e)
    {
        MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
        return 0;
    }

}

DeferApp::DeferApp(HINSTANCE hInstance)
	: D3DApp(hInstance), fg{"frame graph"}
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

	Ubpa::UDX12::DescriptorHeapMngr::Instance().Init(uDevice.raw.Get(), 1024, 1024, 1024, 1024, 1024);

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
		reinterpret_cast<Ubpa::UDX12::FG::RsrcMngr*>(rsrcMngr)->Clear();
	};
	for (auto& frsrc : mFrameResources)
		frsrc->DelayUpdateResource("FrameGraphRsrcMngr", clearFGRsrcMngr);
}

void DeferApp::Update(const GameTimer& gt)
{
    OnKeyboardInput(gt);
	UpdateCamera(gt);

    // Cycle through the circular frame resource array.
    mCurrFrameRsrcMngrIndex = (mCurrFrameRsrcMngrIndex + 1) % gNumFrameResources;
    mCurrFrameRsrcMngr = mFrameResources[mCurrFrameRsrcMngrIndex].get();

    // Has the GPU finished processing the commands of the current frame resource?
    // If not, wait until the GPU has completed commands up to this fence point.
	mCurrFrameRsrcMngr->Wait();

	AnimateMaterials(gt);
	UpdateObjectCBs(gt);
	UpdateMaterialCBs(gt);
	UpdateMainPassCB(gt);
}

void DeferApp::Draw(const GameTimer& gt)
{
	auto cmdListAlloc = mCurrFrameRsrcMngr->GetResource<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>>("CommandAllocator");

    // Reuse the memory associated with command recording.
    // We can only reset when the associated command lists have finished execution on the GPU.
    ThrowIfFailed(cmdListAlloc->Reset());

    // A command list can be reset after it has been added to the command queue via ExecuteCommandList.
    // Reusing the command list reuses memory.
	ThrowIfFailed(uGCmdList->Reset(cmdListAlloc.Get(), nullptr));
	uGCmdList.SetDescriptorHeaps(Ubpa::UDX12::DescriptorHeapMngr::Instance().GetCSUGpuDH()->GetDescriptorHeap());

	uGCmdList->RSSetViewports(1, &mScreenViewport);
	uGCmdList->RSSetScissorRects(1, &mScissorRect);

	fg.Clear();
	auto fgRsrcMngr = mCurrFrameRsrcMngr->GetResource<std::shared_ptr<Ubpa::UDX12::FG::RsrcMngr>>("FrameGraphRsrcMngr");
	fgRsrcMngr->NewFrame();
	fgExecutor.NewFrame();;

	auto gbuffer0 = fg.RegisterResourceNode("GBuffer0");
	auto gbuffer1 = fg.RegisterResourceNode("GBuffer1");
	auto gbuffer2 = fg.RegisterResourceNode("GBuffer2");
	auto backbuffer = fg.RegisterResourceNode("Back Buffer");
	auto depthstencil = fg.RegisterResourceNode("Depth Stencil");
	auto gbPass = fg.RegisterPassNode(
		"GBuffer Pass",
		{},
		{ gbuffer0,gbuffer1,gbuffer2,depthstencil }
	);
	/*auto debugPass = fg.AddPassNode(
		"Debug",
		{ gbuffer1 },
		{ backbuffer }
	);*/
	auto deferLightingPass = fg.RegisterPassNode(
		"Defer Lighting",
		{ gbuffer0,gbuffer1,gbuffer2 },
		{ backbuffer }
	);

	(*fgRsrcMngr)
		.RegisterTemporalRsrc(gbuffer0,
			Ubpa::UDX12::FG::RsrcType::RT2D(DXGI_FORMAT_R32G32B32A32_FLOAT, mClientWidth, mClientHeight, Colors::Black))
		.RegisterTemporalRsrc(gbuffer1,
			Ubpa::UDX12::FG::RsrcType::RT2D(DXGI_FORMAT_R32G32B32A32_FLOAT, mClientWidth, mClientHeight, Colors::Black))
		.RegisterTemporalRsrc(gbuffer2,
			Ubpa::UDX12::FG::RsrcType::RT2D(DXGI_FORMAT_R32G32B32A32_FLOAT, mClientWidth, mClientHeight, Colors::Black))

		.RegisterRsrcTable({
			{gbuffer0,Ubpa::UDX12::Desc::SRV::Tex2D(DXGI_FORMAT_R32G32B32A32_FLOAT)},
			{gbuffer1,Ubpa::UDX12::Desc::SRV::Tex2D(DXGI_FORMAT_R32G32B32A32_FLOAT)},
			{gbuffer2,Ubpa::UDX12::Desc::SRV::Tex2D(DXGI_FORMAT_R32G32B32A32_FLOAT)} })

		.RegisterImportedRsrc(backbuffer, { CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT })
		.RegisterImportedRsrc(depthstencil, { mDepthStencilBuffer.Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE })

		.RegisterPassRsrcs(gbPass, gbuffer0, D3D12_RESOURCE_STATE_RENDER_TARGET,
			Ubpa::UDX12::FG::RsrcImplDesc_RTV_Null{})
		.RegisterPassRsrcs(gbPass, gbuffer1, D3D12_RESOURCE_STATE_RENDER_TARGET,
			Ubpa::UDX12::FG::RsrcImplDesc_RTV_Null{})
		.RegisterPassRsrcs(gbPass, gbuffer2, D3D12_RESOURCE_STATE_RENDER_TARGET,
			Ubpa::UDX12::FG::RsrcImplDesc_RTV_Null{})
		.RegisterPassRsrcs(gbPass, depthstencil,
			D3D12_RESOURCE_STATE_DEPTH_WRITE, Ubpa::UDX12::Desc::DSV::Basic(mDepthStencilFormat))

		/*.RegisterPassRsrcs(debugPass, gbuffer1, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			Ubpa::UDX12::Desc::SRV::Tex2D(DXGI_FORMAT_R32G32B32A32_FLOAT))

		.RegisterPassRsrcs(debugPass, backbuffer, D3D12_RESOURCE_STATE_RENDER_TARGET,
			Ubpa::UDX12::FG::RsrcImplDesc_RTV_Null{})*/

		.RegisterPassRsrcs(deferLightingPass, gbuffer0, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			Ubpa::UDX12::Desc::SRV::Tex2D(DXGI_FORMAT_R32G32B32A32_FLOAT))
		.RegisterPassRsrcs(deferLightingPass, gbuffer1, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			Ubpa::UDX12::Desc::SRV::Tex2D(DXGI_FORMAT_R32G32B32A32_FLOAT))
		.RegisterPassRsrcs(deferLightingPass, gbuffer2, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			Ubpa::UDX12::Desc::SRV::Tex2D(DXGI_FORMAT_R32G32B32A32_FLOAT))

		.RegisterPassRsrcs(deferLightingPass, backbuffer, D3D12_RESOURCE_STATE_RENDER_TARGET,
			Ubpa::UDX12::FG::RsrcImplDesc_RTV_Null{})
		;

	fgExecutor.RegisterPassFunc(
		gbPass,
		[&](const Ubpa::UDX12::FG::PassRsrcs& rsrcs) {
			uGCmdList->SetPipelineState(Ubpa::DXRenderer::Instance().GetPSO("geometry"));
			auto gb0 = rsrcs.find(gbuffer0)->second;
			auto gb1 = rsrcs.find(gbuffer1)->second;
			auto gb2 = rsrcs.find(gbuffer2)->second;
			auto ds = rsrcs.find(depthstencil)->second;

			// Clear the render texture and depth buffer.
			uGCmdList.ClearRenderTargetView(gb0.cpuHandle, Colors::Black);
			uGCmdList.ClearRenderTargetView(gb1.cpuHandle, Colors::Black);
			uGCmdList.ClearRenderTargetView(gb2.cpuHandle, Colors::Black);
			uGCmdList.ClearDepthStencilView(ds.cpuHandle);

			// Specify the buffers we are going to render to.
			std::array rts{ gb0.cpuHandle,gb1.cpuHandle,gb2.cpuHandle };
			uGCmdList->OMSetRenderTargets(rts.size(), rts.data(), false, &ds.cpuHandle);

			uGCmdList->SetGraphicsRootSignature(Ubpa::DXRenderer::Instance().GetRootSignature("geometry"));

			auto passCB = mCurrFrameRsrcMngr
				->GetResource<Ubpa::UDX12::ArrayUploadBuffer<PassConstants>>("gbPass constants")
				.GetResource();
			uGCmdList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());

			DrawRenderItems(uGCmdList.raw.Get(), mOpaqueRitems);
		}
	);

	//fgExecutor.RegisterPassFunc(
	//	debugPass,
	//	[&](const Ubpa::UDX12::FG::PassRsrcs& rsrcs) {
	//		uGCmdList->SetPipelineState(Ubpa::DXRenderer::Instance().GetPSO("screen"));
	//		auto img = rsrcs.find(gbuffer1)->second;
	//		auto bb = rsrcs.find(backbuffer)->second;
	//		
	//		//uGCmdList->CopyResource(bb.resource, rt.resource);

	//		// Clear the render texture and depth buffer.
	//		uGCmdList.ClearRenderTargetView(bb.cpuHandle, Colors::LightSteelBlue);

	//		// Specify the buffers we are going to render to.
	//		//uGCmdList.OMSetRenderTarget(bb.cpuHandle, ds.cpuHandle);
	//		uGCmdList->OMSetRenderTargets(1, &bb.cpuHandle, false, nullptr);

	//		uGCmdList->SetGraphicsRootSignature(Ubpa::DXRenderer::Instance().GetRootSignature("screen"));

	//		uGCmdList->SetGraphicsRootDescriptorTable(0, img.gpuHandle);

	//		uGCmdList->IASetVertexBuffers(0, 0, nullptr);
	//		uGCmdList->IASetIndexBuffer(nullptr);
	//		uGCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	//		uGCmdList->DrawInstanced(6, 1, 0, 0);
	//	}
	//);

	fgExecutor.RegisterPassFunc(
		deferLightingPass,
		[&](const Ubpa::UDX12::FG::PassRsrcs& rsrcs) {
			uGCmdList->SetPipelineState(Ubpa::DXRenderer::Instance().GetPSO("defer lighting"));
			auto gb0 = rsrcs.find(gbuffer0)->second;
			auto gb1 = rsrcs.find(gbuffer1)->second;
			auto gb2 = rsrcs.find(gbuffer2)->second;

			auto bb = rsrcs.find(backbuffer)->second;

			//uGCmdList->CopyResource(bb.resource, rt.resource);

			// Clear the render texture and depth buffer.
			uGCmdList.ClearRenderTargetView(bb.cpuHandle, Colors::LightSteelBlue);

			// Specify the buffers we are going to render to.
			//uGCmdList.OMSetRenderTarget(bb.cpuHandle, ds.cpuHandle);
			uGCmdList->OMSetRenderTargets(1, &bb.cpuHandle, false, nullptr);

			uGCmdList->SetGraphicsRootSignature(Ubpa::DXRenderer::Instance().GetRootSignature("defer lighting"));

			uGCmdList->SetGraphicsRootDescriptorTable(0, gb0.gpuHandle);

			uGCmdList->IASetVertexBuffers(0, 0, nullptr);
			uGCmdList->IASetIndexBuffer(nullptr);
			uGCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			uGCmdList->DrawInstanced(6, 1, 0, 0);
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

	mCurrFrameRsrcMngr->Signal(uCmdQueue.raw.Get(), ++mCurrentFence);
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
	auto& currObjectCB = mCurrFrameRsrcMngr
		->GetResource<Ubpa::UDX12::ArrayUploadBuffer<ObjectConstants>>("ArrayUploadBuffer<ObjectConstants>");
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

			currObjectCB.Set(e->ObjCBIndex, objConstants);

			// Next FrameResource need to be updated too.
			e->NumFramesDirty--;
		}
	}
}

void DeferApp::UpdateMaterialCBs(const GameTimer& gt)
{
	auto& currMaterialCB = mCurrFrameRsrcMngr
		->GetResource<Ubpa::UDX12::ArrayUploadBuffer<MaterialConstants>>("ArrayUploadBuffer<MaterialConstants>");
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

			currMaterialCB.Set(mat->MatCBIndex, matConstants);

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

	auto& currPassCB = mCurrFrameRsrcMngr
		->GetResource<Ubpa::UDX12::ArrayUploadBuffer<PassConstants>>("gbPass constants");
	currPassCB.Set(0, mMainPassCB);
}

void DeferApp::LoadTextures()
{
	std::array<std::wstring_view, 3> ironTextures{
		L"../data/textures/iron/albedo.dds",
		L"../data/textures/iron/roughness.dds",
		L"../data/textures/iron/metalness.dds"
	};

	Ubpa::DXRenderer::Instance().RegisterDDSTextureArrayFromFile(
		Ubpa::DXRenderer::Instance().GetUpload(),
		"iron",
		ironTextures.data(), ironTextures.size());
}

void DeferApp::BuildRootSignature()
{
	{ // geometry
		CD3DX12_DESCRIPTOR_RANGE texTable;
		texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0);

		// Root parameter can be a table, root descriptor or root constants.
		CD3DX12_ROOT_PARAMETER slotRootParameter[4];

		// Perfomance TIP: Order from most frequent to least frequent.
		slotRootParameter[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
		slotRootParameter[1].InitAsConstantBufferView(0);
		slotRootParameter[2].InitAsConstantBufferView(1);
		slotRootParameter[3].InitAsConstantBufferView(2);

		auto staticSamplers = Ubpa::DXRenderer::Instance().GetStaticSamplers();

		// A root signature is an array of root parameters.
		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter,
			(UINT)staticSamplers.size(), staticSamplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		Ubpa::DXRenderer::Instance().RegisterRootSignature("geometry", &rootSigDesc);
	}

	{ // screen
		CD3DX12_DESCRIPTOR_RANGE texTable;
		texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

		// Root parameter can be a table, root descriptor or root constants.
		CD3DX12_ROOT_PARAMETER slotRootParameter[1];

		// Perfomance TIP: Order from most frequent to least frequent.
		slotRootParameter[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);

		auto staticSamplers = Ubpa::DXRenderer::Instance().GetStaticSamplers();

		// A root signature is an array of root parameters.
		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(1, slotRootParameter,
			(UINT)staticSamplers.size(), staticSamplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		Ubpa::DXRenderer::Instance().RegisterRootSignature("screen", &rootSigDesc);
	}
	{ // defer lighting
		CD3DX12_DESCRIPTOR_RANGE texTable;
		texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0);

		// Root parameter can be a table, root descriptor or root constants.
		CD3DX12_ROOT_PARAMETER slotRootParameter[4];

		// Perfomance TIP: Order from most frequent to least frequent.
		slotRootParameter[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
		slotRootParameter[1].InitAsConstantBufferView(0);
		slotRootParameter[2].InitAsConstantBufferView(1);
		slotRootParameter[3].InitAsConstantBufferView(2);

		auto staticSamplers = Ubpa::DXRenderer::Instance().GetStaticSamplers();

		// A root signature is an array of root parameters.
		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter,
			(UINT)staticSamplers.size(), staticSamplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		Ubpa::DXRenderer::Instance().RegisterRootSignature("defer lighting", &rootSigDesc);
	}
}

void DeferApp::BuildDescriptorHeaps()
{
}

void DeferApp::BuildShadersAndInputLayout()
{
	Ubpa::DXRenderer::Instance().RegisterShaderByteCode("standardVS",
		L"..\\data\\shaders\\01_defer\\Default.hlsl", nullptr, "VS", "vs_5_0");
	Ubpa::DXRenderer::Instance().RegisterShaderByteCode("opaquePS",
		L"..\\data\\shaders\\01_defer\\Default.hlsl", nullptr, "PS", "ps_5_0");
	Ubpa::DXRenderer::Instance().RegisterShaderByteCode("screenVS",
		L"..\\data\\shaders\\01_defer\\Screen.hlsl", nullptr, "VS", "vs_5_0");
	Ubpa::DXRenderer::Instance().RegisterShaderByteCode("screenPS",
		L"..\\data\\shaders\\01_defer\\Screen.hlsl", nullptr, "PS", "ps_5_0");
	Ubpa::DXRenderer::Instance().RegisterShaderByteCode("geometryVS",
		L"..\\data\\shaders\\01_defer\\Geometry.hlsl", nullptr, "VS", "vs_5_0");
	Ubpa::DXRenderer::Instance().RegisterShaderByteCode("geometryPS",
		L"..\\data\\shaders\\01_defer\\Geometry.hlsl", nullptr, "PS", "ps_5_0");
	Ubpa::DXRenderer::Instance().RegisterShaderByteCode("deferLightingVS",
		L"..\\data\\shaders\\01_defer\\deferLighting.hlsl", nullptr, "VS", "vs_5_0");
	Ubpa::DXRenderer::Instance().RegisterShaderByteCode("deferLightingPS",
		L"..\\data\\shaders\\01_defer\\deferLighting.hlsl", nullptr, "PS", "ps_5_0");
	
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
 
	Ubpa::UDX12::SubmeshGeometry boxSubmesh;
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

	Ubpa::DXRenderer::Instance()
		.RegisterStaticMeshGeometry(
			Ubpa::DXRenderer::Instance().GetUpload(), "boxGeo",
			vertices.data(), (UINT)vertices.size(), sizeof(Vertex),
			indices.data(), (UINT)indices.size(), DXGI_FORMAT_R16_UINT)
		.submeshGeometries["box"] = boxSubmesh;
}

void DeferApp::BuildPSOs()
{
	auto screenPsoDesc = Ubpa::UDX12::Desc::PSO::Basic(
		Ubpa::DXRenderer::Instance().GetRootSignature("screen"),
		nullptr, 0,
		Ubpa::DXRenderer::Instance().GetShaderByteCode("screenVS"),
		Ubpa::DXRenderer::Instance().GetShaderByteCode("screenPS"),
		mBackBufferFormat,
		DXGI_FORMAT_UNKNOWN
	);
	Ubpa::DXRenderer::Instance().RegisterPSO("screen", &screenPsoDesc);

	auto geometryPsoDesc = Ubpa::UDX12::Desc::PSO::MRT(
		Ubpa::DXRenderer::Instance().GetRootSignature("geometry"),
		mInputLayout.data(), (UINT)mInputLayout.size(),
		Ubpa::DXRenderer::Instance().GetShaderByteCode("geometryVS"),
		Ubpa::DXRenderer::Instance().GetShaderByteCode("geometryPS"),
		3,
		DXGI_FORMAT_R32G32B32A32_FLOAT,
		mDepthStencilFormat
	);
	Ubpa::DXRenderer::Instance().RegisterPSO("geometry", &geometryPsoDesc);

	auto deferLightingPsoDesc = Ubpa::UDX12::Desc::PSO::Basic(
		Ubpa::DXRenderer::Instance().GetRootSignature("defer lighting"),
		nullptr, 0,
		Ubpa::DXRenderer::Instance().GetShaderByteCode("deferLightingVS"),
		Ubpa::DXRenderer::Instance().GetShaderByteCode("deferLightingPS"),
		mBackBufferFormat,
		DXGI_FORMAT_UNKNOWN
	);
	Ubpa::DXRenderer::Instance().RegisterPSO("defer lighting", &deferLightingPsoDesc);
}

void DeferApp::BuildFrameResources()
{
    for(int i = 0; i < gNumFrameResources; ++i)
    {
		auto fr = std::make_unique<Ubpa::UDX12::FrameRsrcMngr>(mCurrentFence, mFence.Get());

		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
		ThrowIfFailed(uDevice->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(&allocator)));

		fr->RegisterResource("CommandAllocator", allocator);

		fr->RegisterResource("gbPass constants",
			Ubpa::UDX12::ArrayUploadBuffer<PassConstants>{ uDevice.raw.Get(), 1, true });

		fr->RegisterResource("ArrayUploadBuffer<MaterialConstants>",
			Ubpa::UDX12::ArrayUploadBuffer<MaterialConstants>{ uDevice.raw.Get(), mMaterials.size(), true });

		fr->RegisterResource("ArrayUploadBuffer<ObjectConstants>",
			Ubpa::UDX12::ArrayUploadBuffer<ObjectConstants>{ uDevice.raw.Get(), mAllRitems.size(), true });

		auto fgRsrcMngr = std::make_shared<Ubpa::UDX12::FG::RsrcMngr>();
		fgRsrcMngr->Init(uGCmdList, uDevice);
		fr->RegisterResource("FrameGraphRsrcMngr", fgRsrcMngr);

		mFrameResources.emplace_back(std::move(fr));
    }
}

void DeferApp::BuildMaterials()
{
	auto woodCrate = std::make_unique<Material>();
	woodCrate->Name = "iron";
	woodCrate->MatCBIndex = 0;
	woodCrate->DiffuseSrvGpuHandle = Ubpa::DXRenderer::Instance().GetTextureSrvGpuHandle("iron");
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
    UINT objCBByteSize = Ubpa::UDX12::Util::CalcConstantBufferByteSize(sizeof(ObjectConstants));
    UINT matCBByteSize = Ubpa::UDX12::Util::CalcConstantBufferByteSize(sizeof(MaterialConstants));
 
	auto objectCB = mCurrFrameRsrcMngr
		->GetResource<Ubpa::UDX12::ArrayUploadBuffer<ObjectConstants>>("ArrayUploadBuffer<ObjectConstants>")
		.GetResource();
	auto matCB = mCurrFrameRsrcMngr
		->GetResource<Ubpa::UDX12::ArrayUploadBuffer<MaterialConstants>>("ArrayUploadBuffer<MaterialConstants>")
		.GetResource();

    // For each render item...
    for(size_t i = 0; i < ritems.size(); ++i)
    {
        auto ri = ritems[i];

        cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
        cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
        cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

        D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex*objCBByteSize;
		D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + ri->Mat->MatCBIndex*matCBByteSize;

		cmdList->SetGraphicsRootDescriptorTable(0, ri->Mat->DiffuseSrvGpuHandle);
        cmdList->SetGraphicsRootConstantBufferView(1, objCBAddress);
        cmdList->SetGraphicsRootConstantBufferView(3, matCBAddress);

        cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
    }
}
