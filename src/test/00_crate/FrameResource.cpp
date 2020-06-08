#include "FrameResource.h"

FrameResource::FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT materialCount)
{
    ThrowIfFailed(device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(CmdListAlloc.GetAddressOf())));

  //  FrameCB = std::make_unique<Ubpa::DX12::ArrayUploadBuffer<FrameConstants>>(device, 1, true);
    PassCB = std::make_unique<Ubpa::DX12::ArrayUploadBuffer<PassConstants>>(device, passCount, true);
    MaterialCB = std::make_unique<Ubpa::DX12::ArrayUploadBuffer<MaterialConstants>>(device, materialCount, true);
    ObjectCB = std::make_unique<Ubpa::DX12::ArrayUploadBuffer<ObjectConstants>>(device, objectCount, true);
}

FrameResource::~FrameResource()
{

}