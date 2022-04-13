#pragma once

#include "glm/vec2.hpp"
#include "glm/vec4.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "VulkanFunctions.h"
#include <vector>
#include <random>
#include <chrono>
#include "Helper.h"
#include "VulkanBase.h"

class PostProcess : public VulkanBase
{
public:

	PostProcess();

	void InitData();
	void InitBuffers();
	void CopyBuffer(VkBuffer& srcBuffer, VkBuffer& dstBuffer, uint32_t& bufferSize);
	void PrepareCopyCommandBuffer();
	void PrepareCombinedImageSampler();
	void SetImageMemoryBarrier(VkImageMemoryBarrier& memoryBarrier, ImageTransition& imageTransition);

	void PrepareDescriptorSets();
	void SetupRenderPass();
	void PrepareGraphicsPipeline();
	void PrepareGraphicsCommandBuffer();
	void CreateFrameBuffers();
	bool PrepareFrame();
	bool SubmitFrame();
	void Draw();

	struct {
		VkPipelineVertexInputStateCreateInfo inputState;
		std::vector<VkVertexInputBindingDescription> bindingDescriptions;
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
	}vertices;

	VkBuffer m_VertexBuffer;
	VkDeviceMemory m_VertexBufferMemory;
	VkCommandBuffer m_CopyCommandBuffer;

	ImageParameters m_ImageParameters;
// 	VkSampler m_Sampler;
// 	VkImage m_Image;
// 	VkDeviceMemory m_ImageMemory;
// 	VkImageView m_ImageView;

// 	VkDescriptorSetLayout m_DescriptorSetLayout;
// 	VkDescriptorPool m_DescriptorPool;
// 	std::vector<VkDescriptorSet> m_DescriptorSets;
// 
// 	VkRenderPass m_RenderPass;
// 	VkPipelineLayout m_GraphicsPipelineLayout;
// 	VkPipeline m_GraphicsPipeline;

	std::vector<float> m_Vertices = {
		// positions         // texcoords
		-1.0f, -1.0f, 0.0f,  0.0f, 0.0f,
		-1.0f,  1.0f, 0.0f,  0.0f, 1.0f,
		 1.0f, -1.0f, 0.0f,  1.0f, 0.0f,
		 1.0f,  1.0f, 0.0f,  1.0f, 1.0f,
	};

};