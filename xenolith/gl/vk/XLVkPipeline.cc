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
		return setup(dev, data, data.data);
	} else if (data.callback) {
		bool ret = false;
		data.callback([&] (SpanView<uint32_t> shaderData) {
			ret = setup(dev, data, shaderData);
		});
		return ret;
	}

	return false;
}

bool Shader::setup(Device &dev, const gl::ProgramData &programData, SpanView<uint32_t> data) {
	VkShaderModuleCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = data.size() * sizeof(uint32_t);
	createInfo.flags = 0;
	createInfo.pCode = data.data();

	if (dev.getTable()->vkCreateShaderModule(dev.getDevice(), &createInfo, nullptr, &_shaderModule) == VK_SUCCESS) {
		return gl::Shader::init(dev, [] (gl::Device *dev, gl::ObjectType, void *ptr) {
			auto d = ((Device *)dev);
			d->getTable()->vkDestroyShaderModule(d->getDevice(), (VkShaderModule)ptr, nullptr);
		}, gl::ObjectType::ShaderModule, _shaderModule);
	}
	return false;
}

bool Pipeline::comparePipelineOrdering(const gl::PipelineInfo &l, const gl::PipelineInfo &r) {
	if (l.material.depth.writeEnabled != r.material.depth.writeEnabled) {
		if (l.material.depth.writeEnabled) {
			return true; // pipelines with depth write comes first
		}
		return false;
	} else if (l.material.blend.enabled != r.material.blend.enabled) {
		if (!l.material.blend.enabled) {
			return true; // pipelines without blending comes first
		}
		return false;
	} else {
		return &l < &r;
	}
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

	if (params.material.lineWidth == 0.0f) {
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizer.lineWidth = 1.0f;
	} else if (params.material.lineWidth > 0.0f) {
		rasterizer.polygonMode = VK_POLYGON_MODE_LINE;
		rasterizer.lineWidth = params.material.lineWidth;
	} else if (params.material.lineWidth < 0.0f) {
		rasterizer.polygonMode = VK_POLYGON_MODE_POINT;
		rasterizer.lineWidth = - params.material.lineWidth;
	}

	rasterizer.cullMode = VK_CULL_MODE_NONE;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
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

	Vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments;
	for (size_t i = 0; i < pass.outputImages.size(); ++ i) {
		colorBlendAttachments.emplace_back(VkPipelineColorBlendAttachmentState{
			params.material.blend.isEnabled() ? VK_TRUE : VK_FALSE,
			VkBlendFactor(params.material.blend.srcColor),
			VkBlendFactor(params.material.blend.dstColor),
			VkBlendOp(params.material.blend.opColor),
			VkBlendFactor(params.material.blend.srcAlpha),
			VkBlendFactor(params.material.blend.dstAlpha),
			VkBlendOp(params.material.blend.opAlpha),
			VkColorComponentFlags(params.material.blend.writeMask),
		});
	}

	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
	colorBlending.attachmentCount = colorBlendAttachments.size();
	colorBlending.pAttachments = colorBlendAttachments.data();
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

	struct SpecInfo {
		VkSpecializationInfo specInfo;
		Vector<VkSpecializationMapEntry> entries;
		Bytes data;
	};

	Vector<SpecInfo> specs;
	size_t constants = 0;
	for (auto &shader : params.shaders) {
		if (!shader.constants.empty()) {
			++ constants;
		}
	}
	specs.reserve(constants);

	for (auto &shader : params.shaders) {
		VkPipelineShaderStageCreateInfo info;
		info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		info.pNext = nullptr;
		info.flags = 0;
		info.stage = VkShaderStageFlagBits(shader.data->stage);
		info.module = shader.data->program.cast<Shader>()->getModule();
		info.pName = "main";

		if (!shader.constants.empty()) {
			auto &spec = specs.emplace_back();
			spec.entries.reserve(shader.constants.size());
			spec.data.reserve(sizeof(uint32_t) * shader.constants.size());
			uint32_t idx = 0;
			for (auto &it : shader.constants) {
				VkSpecializationMapEntry &entry = spec.entries.emplace_back();
				entry.constantID = idx;
				entry.offset = spec.data.size();
				auto data = dev.emplaceConstant(it, spec.data);
				entry.size = data.size();
				++ idx;
			}

			spec.specInfo.mapEntryCount = spec.entries.size();
			spec.specInfo.pMapEntries = spec.entries.data();
			spec.specInfo.dataSize = spec.data.size();
			spec.specInfo.pData = spec.data.data();
			info.pSpecializationInfo = &spec.specInfo;
		} else {
			info.pSpecializationInfo = nullptr;
		}
		shaderStages.emplace_back(info);
	}

	VkPipelineDepthStencilStateCreateInfo depthState;
	depthState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthState.pNext = nullptr;
	depthState.flags = 0;
	if (pass.depthStencil) {
		if (gl::isDepthFormat(pass.depthStencil->getInfo().format)) {
			depthState.depthTestEnable = params.material.depth.testEnabled ? VK_TRUE : VK_FALSE;
			depthState.depthWriteEnable = params.material.depth.writeEnabled ? VK_TRUE : VK_FALSE;
			depthState.depthCompareOp = VkCompareOp(params.material.depth.compare);

			depthState.depthBoundsTestEnable = params.material.bounds.enabled ? VK_TRUE : VK_FALSE;
			depthState.minDepthBounds = params.material.bounds.min;
			depthState.maxDepthBounds = params.material.bounds.max;
		} else {
			depthState.depthTestEnable = VK_FALSE;
			depthState.depthWriteEnable = VK_FALSE;
			depthState.depthCompareOp = VkCompareOp(0);

			depthState.depthBoundsTestEnable = VK_FALSE;
			depthState.minDepthBounds = 0.0f;
			depthState.maxDepthBounds = 0.0f;
		}

		if (gl::isStencilFormat(pass.depthStencil->getInfo().format)) {
			depthState.stencilTestEnable = params.material.stencil ? VK_TRUE : VK_FALSE;

			depthState.front.failOp = VkStencilOp(params.material.front.fail);
			depthState.front.passOp = VkStencilOp(params.material.front.pass);
			depthState.front.depthFailOp = VkStencilOp(params.material.front.depthFail);
			depthState.front.compareOp = VkCompareOp(params.material.front.compare);
			depthState.front.compareMask = params.material.front.compareMask;
			depthState.front.writeMask = params.material.front.writeMask;
			depthState.front.reference = params.material.front.reference;

			depthState.back.failOp = VkStencilOp(params.material.back.fail);
			depthState.back.passOp = VkStencilOp(params.material.back.pass);
			depthState.back.depthFailOp = VkStencilOp(params.material.back.depthFail);
			depthState.back.compareOp = VkCompareOp(params.material.back.compare);
			depthState.back.compareMask = params.material.back.compareMask;
			depthState.back.writeMask = params.material.back.writeMask;
			depthState.back.reference = params.material.back.reference;
		} else {
			depthState.stencilTestEnable = VK_FALSE;

			depthState.front.failOp = VK_STENCIL_OP_KEEP;
			depthState.front.passOp = VK_STENCIL_OP_KEEP;
			depthState.front.depthFailOp = VK_STENCIL_OP_KEEP;
			depthState.front.compareOp = VK_COMPARE_OP_NEVER;
			depthState.front.compareMask = 0;
			depthState.front.writeMask = 0;
			depthState.front.reference = 0;

			depthState.back.failOp = VK_STENCIL_OP_KEEP;
			depthState.back.passOp = VK_STENCIL_OP_KEEP;
			depthState.back.depthFailOp = VK_STENCIL_OP_KEEP;
			depthState.back.compareOp = VK_COMPARE_OP_NEVER;
			depthState.back.compareMask = 0;
			depthState.back.writeMask = 0;
			depthState.back.reference = 0;
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
	pipelineInfo.pDepthStencilState = pass.depthStencil ? &depthState : nullptr; // Optional
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = (dynamicStates.size() > 0) ? &dynamicState : nullptr; // Optional
	pipelineInfo.layout = pass.renderPass->impl.cast<RenderPassImpl>()->getPipelineLayout();
	pipelineInfo.renderPass = pass.renderPass->impl.cast<RenderPassImpl>()->getRenderPass();
	pipelineInfo.subpass = pass.index;
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
