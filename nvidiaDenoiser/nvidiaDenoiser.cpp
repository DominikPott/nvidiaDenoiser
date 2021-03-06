#include <stdio.h>

#include "Optix\optix_world.h"
#include "OpenImageIO\imageio.h"
#include "OpenImageIO\imagebuf.h"

using namespace OIIO;


int main(int argc, char *argv[])
{
	int result;
	std::cout << "Launching Denoiser." << std::endl;
	
	std::string input_path, albedo_path, normal_path, output_path;
	ImageBuf input_buffer, albedo_buffer, normal_buffer;
	ROI input_roi, albedo_roi, normal_roi;
	int width, albedo_width, normal_width, height, albedo_height, normal_height, nchannels, albedo_nchannels, normal_nchannels;


	for (int i=1; i < argc; i++)
	{
		const std::string arg(argv[i]);
		if (arg == "-i")
		{
			input_path = argv[i + 1];
			std::cout << ">> Beauty Image: " << input_path << std::endl;
			ImageBuf input_buffer = ImageBuf(input_path);
			continue;
		}
		if (arg == "-a")
		{
			albedo_path = argv[i + 1];
			std::cout << ">> Albedo Image: " << albedo_path << std::endl;
			ImageBuf albedo_buffer = ImageBuf(albedo_path);
			continue;
		}
		if (arg == "-n")
		{
			normal_path = argv[i + 1];
			std::cout << ">> Normal Image: " << normal_path << std::endl;
			ImageBuf input_buffer = ImageBuf(normal_path);
			continue;
		}

		if (arg == "-o")
		{
			output_path = argv[i + 1];
			std::cout << "<< Output Image: " << output_path << std::endl;
			continue;
		}
	}

	input_buffer.init_spec(input_path, 0, 0); // otherwise no output image will be writen.
	input_roi = get_roi_full(input_buffer.spec());
	width = input_roi.width();
	height = input_roi.height();
	nchannels = input_roi.nchannels();
	//std::cout << "Beauty" << width << " " << height << " " << nchannels << std::endl;
	std::vector<float> pixels_color(width*height*nchannels);
	result = input_buffer.get_pixels(input_roi, TypeDesc::FLOAT, &pixels_color[0]);
	


	albedo_buffer.init_spec(albedo_path, 0, 0);
	albedo_roi= get_roi_full(albedo_buffer.spec());
	albedo_width = albedo_roi.width();
	albedo_height = albedo_roi.height();
	albedo_nchannels = albedo_roi.nchannels();
	//std::cout << "Albedo" << albedo_width << " " << albedo_height << " " << albedo_nchannels << std::endl;
	std::vector<float> albedo_pixels(albedo_width*albedo_height*albedo_nchannels);
	if (albedo_pixels.size() != 0)
	{
		result = albedo_buffer.get_pixels(albedo_roi, TypeDesc::FLOAT, &albedo_pixels[0]);
	}
	


	normal_buffer.init_spec(normal_path, 0, 0);
	normal_roi = get_roi_full(normal_buffer.spec());
	normal_width= normal_roi.width();
	normal_height = normal_roi.height();
	normal_nchannels = normal_roi.nchannels();
	//std::cout << "Normals" << normal_width << " " << normal_height << " " << normal_nchannels << std::endl;
	std::vector<float> normal_pixels(normal_width*normal_height*normal_nchannels);
	if(normal_pixels.size() != 0)
	{
		result = normal_buffer.get_pixels(normal_roi, TypeDesc::FLOAT, &normal_pixels[0]);
	}


	// Create optix gpu buffer
	optix::Context optix_context = optix::Context::create();
	optix::Buffer ox_input_buffer = optix_context->createBuffer(RT_BUFFER_INPUT_OUTPUT, RT_FORMAT_FLOAT4, width, height);
	optix::Buffer ox_albedo_buffer = optix_context->createBuffer(RT_BUFFER_INPUT_OUTPUT, RT_FORMAT_FLOAT4, albedo_width, albedo_height);
	optix::Buffer ox_normal_buffer = optix_context->createBuffer(RT_BUFFER_INPUT_OUTPUT, RT_FORMAT_FLOAT4, normal_width, normal_height);
	optix::Buffer ox_out_buffer = optix_context->createBuffer(RT_BUFFER_INPUT_OUTPUT, RT_FORMAT_FLOAT4, width, height);
		
	memcpy(ox_input_buffer->map(), &pixels_color[0], sizeof(float)*nchannels*width*height);
	ox_input_buffer->unmap();
	

	if (albedo_pixels.size() != 0)
	{
		memcpy(ox_albedo_buffer->map(), &albedo_pixels[0], sizeof(float)*albedo_nchannels*albedo_width*albedo_height);
		ox_albedo_buffer->unmap();
	}

	if (albedo_pixels.size() != 0 && normal_pixels.size() != 0)
	{
		memcpy(ox_normal_buffer->map(), &normal_pixels[0], sizeof(float)*normal_nchannels*normal_width*normal_height);
		ox_normal_buffer->unmap();
	}


	float blend = 0.0f;
	// copy input buffer into gpu buffer
	optix::PostprocessingStage denoiserStage = optix_context->createBuiltinPostProcessingStage("DLDenoiser");
	denoiserStage->declareVariable("input_buffer")->set(ox_input_buffer);
	denoiserStage->declareVariable("output_buffer")->set(ox_out_buffer);
	denoiserStage->declareVariable("blend")->setFloat(blend);
	denoiserStage->declareVariable("input_albedo_buffer")->set(ox_albedo_buffer);
	denoiserStage->declareVariable("input_normal_buffer")->set(ox_normal_buffer);


	
	optix::CommandList commandList = optix_context->createCommandList();
	commandList->appendPostprocessingStage(denoiserStage, width, height);
	commandList->finalize();
	
	optix_context->validate();
	optix_context->compile();
	

	//Execute denoiser
	std::cout << "Denoising. . . ." << std::endl;
	
	commandList->execute();
	std::cout << "Denoising completetd" << std::endl;

	memcpy(&pixels_color[0], ox_out_buffer->map(), sizeof(float)*nchannels*width*height);
	ox_out_buffer->unmap();
	
	ox_input_buffer->destroy();
	ox_out_buffer->destroy();
	ox_albedo_buffer->destroy();
	ox_normal_buffer->destroy();

	
	result = input_buffer.set_pixels(input_roi, TypeDesc::FLOAT, &pixels_color[0]);
	// write out denoised image
	result = input_buffer.write(output_path);
	std::cout << "Write result: " << result << std::endl;
	
	return EXIT_SUCCESS;
}