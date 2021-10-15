/**
 Copyright (c) 2021 Roman Katuntsev <sbkarr@stappler.org>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 **/

#include "XLVkPipeline.h"
#include "XLGlRenderQueue.h"

namespace stappler::xenolith::vk {

bool Shader::init(Device &dev, const gl::ProgramData &data) {
	_stage = data.stage;
	_name = data.key.str();

	if (!data.data.empty()) {
		VkShaderModuleCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = data.data.size() * sizeof(uint32_t);
		createInfo.flags = 0;
		createInfo.pCode = data.data.data();

		if (dev.getTable()->vkCreateShaderModule(dev.getDevice(), &createInfo, nullptr, &_shaderModule) == VK_SUCCESS) {
			return gl::Shader::init(dev, [] (gl::Device *dev, gl::ObjectType, void *ptr) {
				auto d = ((Device *)dev);
				d->getTable()->vkDestroyShaderModule(d->getDevice(), (VkShaderModule)ptr, nullptr);
			}, gl::ObjectType::ShaderModule, _shaderModule);
		}
	} else if (data.callback) {
		bool ret = false;
		data.callback([&] (SpanView<uint32_t> data) {
			VkShaderModuleCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			createInfo.codeSize = data.size() * sizeof(uint32_t);
			createInfo.flags = 0;
			createInfo.pCode = data.data();

			if (dev.getTable()->vkCreateShaderModule(dev.getDevice(), &createInfo, nullptr, &_shaderModule) == VK_SUCCESS) {
				ret = gl::Shader::init(dev, [] (gl::Device *dev, gl::ObjectType, void *ptr) {
					auto d = ((Device *)dev);
					d->getTable()->vkDestroyShaderModule(d->getDevice(), (VkShaderModule)ptr, nullptr);
				}, gl::ObjectType::ShaderModule, _shaderModule);
			}
		});
		return ret;
	}

	return false;
}

bool Pipeline::init(Device &dev, const gl::PipelineData &params, const gl::RenderSubpassData &pass, const gl::RenderQueue &queue) {
	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.pNext = nullptr;
	vertexInputInfo.vertexBindingDescriptionCount = 0;
	vertexInputInfo.pVertexBindingDescriptions = nullptr; // Optional
	vertexInputInfo.vertexAttributeDescriptionCount = 0;
	vertexInputInfo.pVertexAttributeDescriptions = nullptr; // Optional

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.pNext = nullptr;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = 0.0f;
	viewport.height = 0.0f;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor{};
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = 0;
	scissor.extent.height = 0;

	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.pNext = nullptr;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.pNext = nullptr;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	//rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.cullMode = VK_CULL_MODE_NONE;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	//rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f; // Optional
	rasterizer.depthBiasClamp = 0.0f; // Optional
	rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.pNext = nullptr;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.minSampleShading = 1.0f; // Optional
	multisampling.pSampleMask = nullptr; // Optional
	multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
	multisampling.alphaToOneEnable = VK_FALSE; // Optional

	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
	colorBlending.blendConstants[0] = 0.0f; // Optional
	colorBlending.blendConstants[1] = 0.0f; // Optional
	colorBlending.blendConstants[2] = 0.0f; // Optional
	colorBlending.blendConstants[3] = 0.0f; // Optional

	Vector<VkDynamicState> dynamicStates;

	if ((params.dynamicState & gl::DynamicState::Viewport) != gl::DynamicState::None) {
		dynamicStates.emplace_back(VK_DYNAMIC_STATE_VIEWPORT);
	}

	if ((params.dynamicState & gl::DynamicState::Scissor) != gl::DynamicState::None) {
		dynamicStates.emplace_back(VK_DYNAMIC_STATE_SCISSOR);
	}

	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = dynamicStates.size();
	dynamicState.pDynamicStates = dynamicStates.data();

	Vector<VkPipelineShaderStageCreateInfo> shaderStages; shaderStages.reserve(params.shaders.size());

	for (auto &it : params.shaders) {
		if (auto shader = queue.getProgram(it)) {
			VkPipelineShaderStageCreateInfo info;
			info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			info.pNext = nullptr;
			info.flags = 0;
			info.stage = VkShaderStageFlagBits(shader->stage);
			info.module = shader->program.cast<Shader>()->getModule();
			info.pName = "main";
			info.pSpecializationInfo = nullptr;
			shaderStages.emplace_back(info);
		}
	}

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = shaderStages.size();
	pipelineInfo.pStages = shaderStages.data();
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = nullptr; // Optional
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = (dynamicStates.size() > 0) ? &dynamicState : nullptr; // Optional
	pipelineInfo.layout = pass.renderPass->impl.cast<RenderPassImpl>()->getPipelineLayout();
	pipelineInfo.renderPass = pass.renderPass->impl.cast<RenderPassImpl>()->getRenderPass();
	pipelineInfo.subpass = pass.index; // FIXME: bind pipelines to subpass instead of pass
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = -1;

	if (dev.getTable()->vkCreateGraphicsPipelines(dev.getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_pipeline) == VK_SUCCESS) {
		_name = params.key.str();
		return gl::Pipeline::init(dev, [] (gl::Device *dev, gl::ObjectType, void *ptr) {
			auto d = ((Device *)dev);
			d->getTable()->vkDestroyPipeline(d->getDevice(), (VkPipeline)ptr, nullptr);
		}, gl::ObjectType::Pipeline, _pipeline);
	}
	return false;
}

}
