#include "PostProcess.h"

PostProcess::PostProcess()
{

	if (!prepareVulkan()) {
		throw std::runtime_error("FAILED TO PREPARE VULKAN");
	}
	InitBuffers();
	PrepareCombinedImageSampler();
	PrepareDescriptorSets();
	SetupRenderPass();
	PrepareGraphicsPipeline();
	PrepareGraphicsCommandBuffer();
}

void PostProcess::InitData(){

}

void PostProcess::InitBuffers()
{
	//-------------VERTEX BUFFER---------------//
	//Setting Buffer Parameters for Buffer Creation
	Buffer buffer;
	BufferParameters bufferParameters{};
	bufferParameters.bufferHandle = &m_VertexBuffer;
	bufferParameters.formatFeaturesBits = VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT  & VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
	bufferParameters.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	bufferParameters.usageBits = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	bufferParameters.size = sizeof(m_Vertices[0]) * m_Vertices.size();

	VkMemoryPropertyFlags memoryPropertiesBits = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	buffer.CreateBuffer(m_PhysicalDevice, m_Device, bufferParameters, memoryPropertiesBits, m_VertexBufferMemory);

	//-------------STAGING BUFFER---------------//
	VkBuffer stagingBuffer;
	bufferParameters.bufferHandle = &stagingBuffer;
	bufferParameters.formatFeaturesBits = VK_FORMAT_FEATURE_TRANSFER_SRC_BIT;
	bufferParameters.usageBits = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	bufferParameters.size = sizeof(m_Vertices[0]) * m_Vertices.size();

	VkDeviceMemory stagingBufferMemoryObject;
	memoryPropertiesBits = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
	buffer.CreateBuffer(m_PhysicalDevice, m_Device, bufferParameters, memoryPropertiesBits, stagingBufferMemoryObject);
	
	//****** Memory Mapping ******//
	void* data;
	vkMapMemory(m_Device, stagingBufferMemoryObject, 0, bufferParameters.size, 0, &data);
	memcpy(data, m_Vertices.data(), (size_t)bufferParameters.size);
	vkUnmapMemory(m_Device, stagingBufferMemoryObject);

	//****** Copying staging Buffer data(Host Visible) to Storage Texel Buffer (Device Local) ******//
	CopyBuffer(stagingBuffer, m_VertexBuffer, bufferParameters.size);
	vkDestroyBuffer(m_Device, stagingBuffer, nullptr);
	vkFreeMemory(m_Device, stagingBufferMemoryObject, nullptr);
}

void PostProcess::CopyBuffer(VkBuffer& srcBuffer, VkBuffer& dstBuffer, uint32_t& bufferSize)
{
	CreateCommandPool(m_Device, m_PresentationQueueFamilyIndex, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, m_PresentQueueCommandPool);
	VkCommandBufferAllocateInfo allocateInfo = {};
	allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocateInfo.commandPool = m_PresentQueueCommandPool;
	allocateInfo.commandBufferCount = 1;

	VkCommandBuffer copyCommandBuffer;
	vkAllocateCommandBuffers(m_Device, &allocateInfo, &copyCommandBuffer);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	beginInfo.pNext = nullptr;
	beginInfo.pInheritanceInfo = nullptr;

	//******* Recording Command Buffer for Copy *******
	vkBeginCommandBuffer(copyCommandBuffer, &beginInfo);

	VkBufferCopy copyRegion{};
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	copyRegion.size = bufferSize;

	vkCmdCopyBuffer(copyCommandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

	flushCommandBuffer(m_Device, copyCommandBuffer, m_GraphicsQueue, m_PresentQueueCommandPool, true);
	vkQueueWaitIdle(m_GraphicsQueue);

}

void PostProcess::PrepareCopyCommandBuffer()
{
	CreateCommandPool(m_Device, m_GraphicsQueueFamilyIndex, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, graphics.commandpool);

	CreateCommandBuffer(m_Device, graphics.commandpool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1, &m_CopyCommandBuffer);

}

void PostProcess::PrepareCombinedImageSampler()
{
	int width = 1;
	int height = 1;
	std::vector<unsigned char> image_data;
	if (!LoadTextureDataFromFile("sunset.jpg", 4, image_data, &width, &height)) {
		throw std::runtime_error("COULD NOT LOAD IMAGE");
	}

	VkSamplerCreateInfo samplerCreateInfo = {};
	samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCreateInfo.pNext = nullptr;
	samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
	samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.mipLodBias = 0.0f;
	samplerCreateInfo.anisotropyEnable = false;
	samplerCreateInfo.maxAnisotropy = 1.0f;
	samplerCreateInfo.compareEnable = 0.0f;
	samplerCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerCreateInfo.minLod = 0.0f;
	samplerCreateInfo.maxLod = 0.0f;
	samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
	samplerCreateInfo.unnormalizedCoordinates = true;

	if (vkCreateSampler(m_Device, &samplerCreateInfo, nullptr, &m_ImageParameters.Sampler) !=VK_SUCCESS) {
		throw std::runtime_error("COULD NOT CREATE SAMPLER");
	}

	VkFormatProperties format_Properties;

	vkGetPhysicalDeviceFormatProperties(m_PhysicalDevice, VK_FORMAT_R8G8B8A8_UNORM, &format_Properties);
	if (!(format_Properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) {
		throw std::runtime_error(" Provided format not supported for IMAGE");
	}

	VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
	VkImageAspectFlagBits aspects = VK_IMAGE_ASPECT_COLOR_BIT;

	VkImageCreateInfo imageCreateInfo = {};
	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCreateInfo.pNext = nullptr;
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.format = format;
	imageCreateInfo.extent = { (uint32_t)width, (uint32_t)height, 1 };
	imageCreateInfo.mipLevels = 1.0f;
	imageCreateInfo.arrayLayers = 1.0f;
	imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	if (vkCreateImage(m_Device, &imageCreateInfo, nullptr, &m_ImageParameters.Handle) != VK_SUCCESS) {
		throw std::runtime_error("COULD NOT CREATE IMAGE");
	}

	//TO ALLOCATE MEMORY FOR IMAGE AND BIND MEMORY_OBJECT
	Buffer imageBuffer;
	VkMemoryPropertyFlags memoryPropertiesBits = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	imageBuffer.AllocateAndBindMemoryObjectToImage(m_PhysicalDevice, m_Device, memoryPropertiesBits, m_ImageParameters.Handle, m_ImageParameters.Memory);
	
	VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageBuffer.CreateImageView(m_Device, m_ImageParameters.Handle, viewType, format, aspects, m_ImageParameters.View);

	VkBuffer stagingBuffer;
	UpdateImageBufferUsingStagingBuffer(m_PhysicalDevice, m_Device, static_cast<VkDeviceSize>(image_data.size()), image_data.data(), stagingBuffer, m_ImageParameters.Handle);

	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	beginInfo.pNext = nullptr;
	beginInfo.pInheritanceInfo = nullptr;

	ImageTransition imageTransition;
	//******* Recording Command Buffer for Copy *******
	PrepareCopyCommandBuffer();
	vkBeginCommandBuffer(m_CopyCommandBuffer, &beginInfo);

	imageTransition.image = m_ImageParameters.Handle;
	imageTransition.currentAccess = 0;
	imageTransition.newAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
	imageTransition.currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageTransition.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

	VkImageMemoryBarrier imageMemoryBarrier = {};
	
	SetImageMemoryBarrier(imageMemoryBarrier, imageTransition);
	
	vkCmdPipelineBarrier(m_CopyCommandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
	// copy data 
	VkBufferImageCopy copyRegion;
	copyRegion.bufferOffset = 0;
	copyRegion.bufferRowLength = 0;
	copyRegion.bufferImageHeight = 0;
	copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copyRegion.imageSubresource.mipLevel = 0;
	copyRegion.imageSubresource.layerCount = 1;
	copyRegion.imageSubresource.baseArrayLayer = 0;
	copyRegion.imageExtent = { (uint32_t)width, (uint32_t)height, 1 };
	copyRegion.imageOffset = { 0, 0, 0 };

	vkCmdCopyBufferToImage(m_CopyCommandBuffer, stagingBuffer, m_ImageParameters.Handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

	imageTransition.currentAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
	imageTransition.newAccess = VK_ACCESS_SHADER_READ_BIT;
	imageTransition.currentLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	imageTransition.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	SetImageMemoryBarrier(imageMemoryBarrier,imageTransition);
	vkCmdPipelineBarrier(m_CopyCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
	
	flushCommandBuffer(m_Device, m_CopyCommandBuffer, m_GraphicsQueue, m_PresentQueueCommandPool, false);
	vkQueueWaitIdle(m_GraphicsQueue);
	
}

void PostProcess::SetImageMemoryBarrier( VkImageMemoryBarrier& memoryBarrier, ImageTransition& imageTransition)
{
	memoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	memoryBarrier.srcAccessMask = imageTransition.currentAccess;
	memoryBarrier.dstAccessMask = imageTransition.newAccess;
	memoryBarrier.oldLayout = imageTransition.currentLayout;
	memoryBarrier.newLayout = imageTransition.newLayout;
	memoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	memoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	memoryBarrier.image = imageTransition.image;
 	memoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	memoryBarrier.subresourceRange.baseMipLevel = 0;
	memoryBarrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	memoryBarrier.subresourceRange.baseArrayLayer = 0;
	memoryBarrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
}

void PostProcess::PrepareDescriptorSets()
{
	std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBindings;
	// Binding = 0 : Combined Image Sampler 
	descriptorSetLayoutBindings.push_back({
		0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr
		});

	VkDescriptorSetLayoutCreateInfo descriptorLayout;
	descriptorLayout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorLayout.pNext = nullptr;
	descriptorLayout.flags = 0;
	descriptorLayout.bindingCount = descriptorSetLayoutBindings.size();
	descriptorLayout.pBindings = descriptorSetLayoutBindings.data();

	if ((vkCreateDescriptorSetLayout(m_Device, &descriptorLayout, nullptr, &m_descriptorSetLayout)) != VK_SUCCESS) {
		throw std::runtime_error("COULD NOT CREATE DESCRIPTOR SET LAYOUT");
	}

	std::vector<VkDescriptorPoolSize> descriptorPoolSize;
	descriptorPoolSize.push_back(
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 }
	);

	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo;
	descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.pNext = nullptr;
	descriptorPoolCreateInfo.flags = 0;
	descriptorPoolCreateInfo.maxSets = 1;
	descriptorPoolCreateInfo.poolSizeCount = static_cast<uint32_t>(descriptorPoolSize.size());
	descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSize.data();

	if ((vkCreateDescriptorPool(m_Device, &descriptorPoolCreateInfo, nullptr, &m_DescriptorPool)) != VK_SUCCESS) {
		throw std::runtime_error("COULD NOT CREATE DESCRIPTOR POOL");
	}

	//Allocating descriptor sets from Descriptor Pool
	VkDescriptorSetAllocateInfo allocInfo;
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.pNext = nullptr;
	allocInfo.descriptorPool = m_DescriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &m_descriptorSetLayout;

	m_descriptorSet.resize(1);
	if ((vkAllocateDescriptorSets(m_Device, &allocInfo, m_descriptorSet.data())) != VK_SUCCESS) {
		throw std::runtime_error("COULD NOT ALLOCATE DESCRIPTOT SETS FROM DESCRITOR POOL");
	}

	//Updating descriptor sets with buffers
	std::vector<VkDescriptorImageInfo> imageInfo;
	imageInfo.resize(1);
	imageInfo[0].sampler = m_ImageParameters.Sampler;
	imageInfo[0].imageView = m_ImageParameters.View;
	imageInfo[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	std::vector<VkWriteDescriptorSet> writeDescriptorSet;
	writeDescriptorSet.resize(1);

	writeDescriptorSet[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDescriptorSet[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writeDescriptorSet[0].descriptorCount = 1;
	writeDescriptorSet[0].dstSet = m_descriptorSet.at(0);
	writeDescriptorSet[0].dstBinding = 0;
	writeDescriptorSet[0].pImageInfo = imageInfo.data();
	writeDescriptorSet[0].descriptorCount = imageInfo.size();

	vkUpdateDescriptorSets(m_Device, writeDescriptorSet.size(), writeDescriptorSet.data(), 0, nullptr);
}

void PostProcess::SetupRenderPass()
{
	std::array<VkAttachmentDescription, 1> attachments;

	//Color Attachment
	attachments[0].flags = 0;
	attachments[0].format = swapChainParameters.Format;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	attachments[0].format = VK_FORMAT_R8G8B8A8_UNORM;

	VkAttachmentReference colorAttachmentReference;
	colorAttachmentReference.attachment = 0;
	colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpassDescription;
	subpassDescription.flags = 0;
	subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDescription.colorAttachmentCount = 1;
	subpassDescription.pColorAttachments = &colorAttachmentReference;
	subpassDescription.inputAttachmentCount = 0;
	subpassDescription.pDepthStencilAttachment = nullptr;
	subpassDescription.pInputAttachments = nullptr;
	subpassDescription.preserveAttachmentCount = 0;
	subpassDescription.pPreserveAttachments = nullptr;
	subpassDescription.pResolveAttachments = nullptr;

	//Subpass dependencies for layout transitions
	std::array<VkSubpassDependency, 2> dependencies;

	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpassDescription;
	renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
	renderPassInfo.pDependencies = dependencies.data();

	if (vkCreateRenderPass(m_Device, &renderPassInfo, nullptr, &m_RenderPass) != VK_SUCCESS) {
		throw std::runtime_error("failed to create RENDER PASS!");
	}
}

void PostProcess::PrepareGraphicsPipeline()
{
	auto vertShaderModule = loadShader("Shaders/shader.vert.spv", m_Device);
	auto fragShaderModule = loadShader("Shaders/shader.frag.spv", m_Device);

	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;
	shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStages[0].pNext = nullptr;
	shaderStages[0].flags = 0;
	shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	shaderStages[0].pName = "main";
	shaderStages[0].module = vertShaderModule;
	shaderStages[0].pSpecializationInfo = nullptr;

	shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStages[1].pNext = nullptr;
	shaderStages[1].flags = 0;
	shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	shaderStages[1].pName = "main";
	shaderStages[1].module = fragShaderModule;
	shaderStages[1].pSpecializationInfo = nullptr;

	//VERTEX INPUT BINDING DESCRIPTION
	vertices.bindingDescriptions.resize(1);
	vertices.bindingDescriptions[0].binding = 0;
	vertices.bindingDescriptions[0].stride = 5 * sizeof(float);
	vertices.bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	//VERTEX ATTRIBUTE DESCRIPTION from the given binding
	//(index at which vertex buffer is bind i.e from which binding to retrieve these attributes)
	vertices.attributeDescriptions.resize(2);

	// Position : Location = 0
	vertices.attributeDescriptions[0].binding = 0;
	vertices.attributeDescriptions[0].location = 0;
	vertices.attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	vertices.attributeDescriptions[0].offset = 0;

	// TextCoords : Location = 1
	vertices.attributeDescriptions[1].binding = 0;
	vertices.attributeDescriptions[1].location = 1;
	vertices.attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
	vertices.attributeDescriptions[1].offset = 3 * sizeof(float);

	// VERTEX INPUT STATE assigned to vertex buffer
	vertices.inputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertices.inputState.pNext = nullptr;
	vertices.inputState.flags = 0;
	vertices.inputState.vertexBindingDescriptionCount = static_cast<uint32_t>(vertices.bindingDescriptions.size());
	vertices.inputState.pVertexBindingDescriptions = vertices.bindingDescriptions.data();
	vertices.inputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertices.attributeDescriptions.size());
	vertices.inputState.pVertexAttributeDescriptions = vertices.attributeDescriptions.data();

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo{};
	inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyStateCreateInfo.pNext = nullptr;
	inputAssemblyStateCreateInfo.flags = 0;
	inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

	std::vector<VkViewport> viewPort;
	viewPort.resize(1);
	viewPort[0].x = 0.0f;
	viewPort[0].y = 0.0f;
	//kept Same as SwapChain ImageExtent
	viewPort[0].width = (float)m_SwapChainExent.width;
	viewPort[0].height = (float)m_SwapChainExent.height;
	viewPort[0].minDepth = 0.0f;
	viewPort[0].maxDepth = 1.0f;

	std::vector<VkRect2D> scissor;
	scissor.resize(viewPort.size());
	scissor[0].offset = { 0,0 };
	scissor[0].extent = m_SwapChainExent;

	VkPipelineViewportStateCreateInfo viewPortState{};
	viewPortState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewPortState.pNext = nullptr;
	viewPortState.flags = 0;
	viewPortState.viewportCount = viewPort.size();
	viewPortState.pViewports = viewPort.data();
	viewPortState.scissorCount = scissor.size();
	viewPortState.pScissors = scissor.data();

	VkPipelineRasterizationStateCreateInfo rasterizationState{};
	rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationState.pNext = nullptr;
	rasterizationState.flags = 0;
	rasterizationState.depthClampEnable = VK_FALSE;
	rasterizationState.rasterizerDiscardEnable = VK_FALSE;
	rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizationState.lineWidth = 1.0f;
	rasterizationState.cullMode = VK_CULL_MODE_NONE;
	rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizationState.depthBiasEnable = VK_FALSE;

	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.pNext = nullptr;
	multisampling.flags = 0;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineColorBlendAttachmentState blendAttachmentState{};
	blendAttachmentState.colorWriteMask = 0xF;
	blendAttachmentState.blendEnable = VK_FALSE;
	blendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
	blendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	blendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
	blendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
	blendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	blendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
	blendAttachmentState.colorWriteMask = {
		VK_COLOR_COMPONENT_R_BIT | 
		VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT |
		VK_COLOR_COMPONENT_A_BIT };

	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.flags = 0;
	colorBlending.pNext = nullptr;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &blendAttachmentState;
	colorBlending.blendConstants[0] = 1.0f;
	colorBlending.blendConstants[1] = 1.0f;
	colorBlending.blendConstants[2] = 1.0f;
	colorBlending.blendConstants[3] = 1.0f;

// 	std::vector<VkDynamicState> dynamicStates = {
// 		VK_DYNAMIC_STATE_VIEWPORT,
// 		VK_DYNAMIC_STATE_SCISSOR,
// 	};
// 
// 	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
// 	dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
// 	dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();

	// 	A pipeline layout defines the set of resources that can be accessed from shaders of a given pipeline.
	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = 0;
	pipelineLayoutInfo.pPushConstantRanges = nullptr;

	if (vkCreatePipelineLayout(m_Device, &pipelineLayoutInfo, nullptr, &graphics.pipelineLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create GRAPHICS pipeline layout!");
	}

	VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.pVertexInputState = &vertices.inputState;
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
	pipelineCreateInfo.pRasterizationState = &rasterizationState;
	pipelineCreateInfo.pColorBlendState = &colorBlending;
	pipelineCreateInfo.pMultisampleState = &multisampling;
	pipelineCreateInfo.pViewportState = &viewPortState;
	//pipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
	pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
	pipelineCreateInfo.pStages = shaderStages.data();
	pipelineCreateInfo.layout = graphics.pipelineLayout;
	pipelineCreateInfo.subpass = 0;
	pipelineCreateInfo.renderPass = m_RenderPass;
	pipelineCreateInfo.basePipelineIndex = -1;
	pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;

	if ((vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &graphics.pipeline)) != VK_SUCCESS) {
		throw std::runtime_error("failed to create graphics pipeline!");
	}

	vkDestroyShaderModule(m_Device, fragShaderModule, nullptr);
	vkDestroyShaderModule(m_Device, vertShaderModule, nullptr);
}

void PostProcess::PrepareGraphicsCommandBuffer()
{
	CreateCommandPool(m_Device, m_GraphicsQueueFamilyIndex, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, graphics.commandpool);

	m_DrawCmdBuffers.resize(m_SwapChainImages.size());
	CreateCommandBuffer(m_Device, graphics.commandpool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, static_cast<uint32_t>(m_SwapChainImages.size()), m_DrawCmdBuffers.data());

	SetupRenderPass();
	CreateFrameBuffers();

	VkSemaphoreCreateInfo semaphoreCreateInfo{};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	vkCreateSemaphore(m_Device, &semaphoreCreateInfo, nullptr, &graphics.semaphore);

	//Recording Render Passes and Drawing commands
	VkCommandBufferBeginInfo cmdBuffBeginInfo{};
	cmdBuffBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBuffBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

	VkClearValue clearValue{
		{25.0f / 256.0f, 40.0f / 256.0f, 32.0f / 256.0f, 0.0f}
	};

	VkRenderPassBeginInfo renderPassBeginInfo{};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass = m_RenderPass;
	renderPassBeginInfo.renderArea.extent = m_SwapChainExent;
	renderPassBeginInfo.renderArea.offset.x = 0;
	renderPassBeginInfo.renderArea.offset.y = 0;
	renderPassBeginInfo.clearValueCount = 1;
	renderPassBeginInfo.pClearValues = &clearValue;

	VkDeviceSize offsets[] = { 0 };
	for (uint32_t i = 0; i < m_DrawCmdBuffers.size(); i++) {
		// SET TARGET FRAME BUFFER
		renderPassBeginInfo.framebuffer = m_FrameBuffers[i];

		vkBeginCommandBuffer(m_DrawCmdBuffers[i], &cmdBuffBeginInfo);

		vkCmdBeginRenderPass(m_DrawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindDescriptorSets(m_DrawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphics.pipelineLayout, 0, 1, m_descriptorSet.data(), 0, NULL);

		vkCmdBindPipeline(m_DrawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphics.pipeline);
		vkCmdBindVertexBuffers(m_DrawCmdBuffers[i], 0, 1, &m_VertexBuffer, offsets);
		vkCmdDraw(m_DrawCmdBuffers[i], 4, 1, 0, 0);

		vkCmdEndRenderPass(m_DrawCmdBuffers[i]);

		vkEndCommandBuffer(m_DrawCmdBuffers[i]);

	}
}

void PostProcess::CreateFrameBuffers()
{
	CreateSwapChainImageViews();

	std::vector<VkImageView> attachments;
	attachments.resize(1); // As we have only Color attachment

	VkFramebufferCreateInfo frameBufferCreateInfo;
	frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	frameBufferCreateInfo.pNext = nullptr;
	frameBufferCreateInfo.flags = 0;
	frameBufferCreateInfo.renderPass = m_RenderPass;
	frameBufferCreateInfo.attachmentCount = attachments.size();
	frameBufferCreateInfo.pAttachments = attachments.data();
	frameBufferCreateInfo.height = m_SwapChainExent.height;
	frameBufferCreateInfo.width = m_SwapChainExent.width;
	frameBufferCreateInfo.layers = 1;

	m_FrameBuffers.resize(m_SwapChainImages.size());

	for (uint32_t i = 0; i < m_FrameBuffers.size(); i++)
	{
		attachments[0] = m_ImageViews[i];
		if (vkCreateFramebuffer(m_Device, &frameBufferCreateInfo, nullptr, &m_FrameBuffers[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create FRAME BUFFER!");
		}
	}
}

bool PostProcess::PrepareFrame()
{
	// Acquire the next image from the swap chain
	VkResult result = vkAcquireNextImageKHR(m_Device, m_SwapChain, UINT32_MAX, m_ImageAvailableSemaphore, VK_NULL_HANDLE, &m_ImageIndex);

	switch (result)
	{
	case VK_SUCCESS: std::cout << " acquired image" << std::endl;
	case VK_SUBOPTIMAL_KHR:
		break;
	case VK_ERROR_OUT_OF_DATE_KHR:
		return OnWindowSizeChanged();

	default:
		throw std::runtime_error("COULD NOT ACQUIRE SWAPCHAIN IMAGE ");
		return false;
	}
	return true;
}

bool PostProcess::SubmitFrame()
{
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = nullptr;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &m_RenderingFinishedSemaphore;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &m_SwapChain;
	presentInfo.pImageIndices = &m_ImageIndex;

	//	pResults – A pointer to an array of at least swapchainCount element; this parameter is optional and can be set to null,
	//	but if we provide such an array, the result of the presenting operation will be stored in each of its elements, for each swap chain respectively; 
	//	a single value returned by the whole function is the same as the worst result value from all swap chains.
	presentInfo.pResults = nullptr;

	VkResult result = vkQueuePresentKHR(m_PresentQueue, &presentInfo);
	switch (result)
	{
	case VK_SUCCESS: std::cout << " presented" << std::endl;
	case VK_SUBOPTIMAL_KHR:
		break;
	case VK_ERROR_OUT_OF_DATE_KHR:
		return OnWindowSizeChanged();

	default:
		std::runtime_error("COULD NOT ACQUIRE SWAPCHAIN IMAGE ");
		return false;
	}
	return true;
}

void PostProcess::Draw()
{
	PrepareFrame();
	// 	If submitted commands should wait for other commands to end, initialize the vector with pipeline stages at which the queue
	// 	should wait for a corresponding semaphore
	VkPipelineStageFlags graphicsWaitSemaphoreStageMask[] = { VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	VkSemaphore graphicsWaitSemaphores[] = { m_ImageAvailableSemaphore };
	VkSemaphore graphicsSignalSemaphores[] = { m_RenderingFinishedSemaphore };

	//Submit Graphics Commands
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext = nullptr;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &m_DrawCmdBuffers[m_ImageIndex];
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = graphicsWaitSemaphores;
	submitInfo.pWaitDstStageMask = graphicsWaitSemaphoreStageMask;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = graphicsSignalSemaphores;

	if (vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) == VK_SUCCESS) {
		std::cout << "graphics submit successful" << std::endl;
	}
	vkQueueWaitIdle(m_GraphicsQueue);
	// Submitting frame for Presentation
	SubmitFrame();
}

