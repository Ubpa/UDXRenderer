#pragma once

#include <string>

#include <d3d12.h>

namespace DirectX {
	class ResourceUploadBatch;
}

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

		DXRenderer& RegisterDDSTextureFromFile(DirectX::ResourceUploadBatch& upload, std::string name, std::wstring filename);

		D3D12_GPU_DESCRIPTOR_HANDLE GetTextureGpuHandle(const std::string& name) const;

	private:
		struct Impl;
		Impl* pImpl;

		DXRenderer();
		~DXRenderer();
	};
}
