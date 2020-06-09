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

		// support tex2d and tex cube
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

		// [summary]
		// compile shader file to bytecode
		// [arguments]
		// - defines: marco array, end with {NULL, NULL}
		// - - e.g. #define zero 0 <-> D3D_SHADER_MACRO Shader_Macros[] = { "zero", "0", NULL, NULL };
		// - entrypoint: begin function name, like 'main'
		// - target: e.g. cs/ds/gs/hs/ps/vs + _5_ + 0/1
		// [ref] https://docs.microsoft.com/en-us/windows/win32/api/d3dcompiler/nf-d3dcompiler-d3dcompilefromfile
		ID3DBlob* RegisterShaderByteCode(
			std::string name,
			const std::wstring& filename,
			const D3D_SHADER_MACRO* defines,
			const std::string& entrypoint,
			const std::string& target);

		DXRenderer& RegisterRootSignature(
			std::string name,
			const D3D12_ROOT_SIGNATURE_DESC* descs);

		DXRenderer& RegisterPSO(
			std::string name,
			const D3D12_GRAPHICS_PIPELINE_STATE_DESC* desc);

		DXRenderer& RegisterRenderTexture2D(std::string name, UINT width, UINT height, DXGI_FORMAT format);
		DXRenderer& RegisterRenderTextureCube(std::string name, UINT size, DXGI_FORMAT format);

		D3D12_CPU_DESCRIPTOR_HANDLE GetTextureSrvCpuHandle(const std::string& name) const;
		D3D12_GPU_DESCRIPTOR_HANDLE GetTextureSrvGpuHandle(const std::string& name) const;

		DX12::DescriptorHeapAllocation& GetTextureRtvs(const std::string& name) const;

		DX12::MeshGeometry& GetMeshGeometry(const std::string& name) const;

		ID3DBlob* GetShaderByteCode(const std::string& name) const;

		ID3D12RootSignature* GetRootSignature(const std::string& name) const;

		ID3D12PipelineState* GetPSO(const std::string& name) const;

	private:
		struct Impl;
		Impl* pImpl;

		DXRenderer();
		~DXRenderer();
	};
}
