#pragma once

#include <string>

#include <UDX12/UDX12.h>

namespace Ubpa {
	class DXRenderer {
	public:
		static DXRenderer& Instance() noexcept {
			static DXRenderer instance;
			return instance;
		}

		DirectX::ResourceUploadBatch& GetUpload() const;

		DXRenderer& Init(ID3D12Device* device);
		void Release();

		DXRenderer& RegisterDDSTextureFromFile(DirectX::ResourceUploadBatch& upload,
			std::string name, std::wstring filename);

		DX12::MeshGeometry& RegisterStaticMeshGeometry(
			DirectX::ResourceUploadBatch& upload, std::string name,
			const void* vb_data, UINT vb_count, UINT vb_stride,
			const void* ib_data, UINT ib_count, DXGI_FORMAT ib_format);

		DX12::MeshGeometry& RegisterDynamicMeshGeometry(
			std::string name,
			const void* vb_data, UINT vb_count, UINT vb_stride,
			const void* ib_data, UINT ib_count, DXGI_FORMAT ib_format);

		D3D12_GPU_DESCRIPTOR_HANDLE GetTextureGpuHandle(const std::string& name) const;

		DX12::MeshGeometry& GetMeshGeometry(const std::string& name) const;

	private:
		struct Impl;
		Impl* pImpl;

		DXRenderer();
		~DXRenderer();
	};
}
