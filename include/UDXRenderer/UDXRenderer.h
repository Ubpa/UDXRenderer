#pragma once

#include <UDX12/UDX12.h>

#include <array>
#include <string>

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
			std::string name, std::wstring_view filename);
		DXRenderer& RegisterDDSTextureArrayFromFile(DirectX::ResourceUploadBatch& upload,
			std::string name, const std::wstring_view* filenameArr, UINT num);

		UDX12::MeshGeometry& RegisterStaticMeshGeometry(
			DirectX::ResourceUploadBatch& upload, std::string name,
			const void* vb_data, UINT vb_count, UINT vb_stride,
			const void* ib_data, UINT ib_count, DXGI_FORMAT ib_format);

		UDX12::MeshGeometry& RegisterDynamicMeshGeometry(
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

		D3D12_CPU_DESCRIPTOR_HANDLE GetTextureSrvCpuHandle(const std::string& name, UINT index = 0) const;
		D3D12_GPU_DESCRIPTOR_HANDLE GetTextureSrvGpuHandle(const std::string& name, UINT index = 0) const;

		UDX12::DescriptorHeapAllocation& GetTextureRtvs(const std::string& name) const;

		UDX12::MeshGeometry& GetMeshGeometry(const std::string& name) const;

		ID3DBlob* GetShaderByteCode(const std::string& name) const;

		ID3D12RootSignature* GetRootSignature(const std::string& name) const;

		ID3D12PipelineState* GetPSO(const std::string& name) const;

		// 1. point wrap
		// 2. point clamp
		// 3. linear wrap
		// 4. linear clamp
		// 5. anisotropic wrap
		// 6. anisotropic clamp
		std::array<CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers() const;

	private:
		struct Impl;
		Impl* pImpl;

		DXRenderer();
		~DXRenderer();
	};
}
