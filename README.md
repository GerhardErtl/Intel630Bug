# Intel630Bug
Demo of a Bug on Intel 620 and 630 GPUs

# Overview
During the development of a huge rendering System knitted Fabrics we found an Error that occurs on Intel 620 and 630 GPUs. After detailed analysis we found that the error only occurs when the Hull Shader is translated with optimize. The version of the graphics driver does not influence the result. We are currently using the newest driver (30.0.101.1191) but the result is identical with older drivers. The error occurs on Windows 10 and Windows 11. We also tested on Computers with different graphics hardware. The Error does not occur on NVidia or AMD GPUs.
To demonstrate the error we have produced this project.

# Usage
Clone this repository
Open Intel630Bug.sln with Visual Studio 2019 or newer.
Compile in x64 Release Mode.
Run the program

# Result
If you are running on an Intel 620 or 630 GPU you see a flickering image when you move the mouse.
If you are using a different GPU you see a yellow image. 

The reason is that a variable sometimes has the wrong value on Intel GPUs and always correct on other GPUs. The name of the Variable is “mustBe5„. It is set to 5 in the hull shader constant function. The domain shader passes the value to the geometry shader without modification. The geometry shader uses the value to determine the color of the geometry. See DirectX12/Shaders/BezierByGrafic for the shader source code. The rest of the program is just used to set up DirectX12 and create some geometry data to render.

This Error was reported to Intel (incident 05319674). Intel chose to ignore the problem and sent me to the discussion forums since it is a “Software Bug”. This is absolutely correct: The bug is in the device driver for Intel 620 GPUs. 


