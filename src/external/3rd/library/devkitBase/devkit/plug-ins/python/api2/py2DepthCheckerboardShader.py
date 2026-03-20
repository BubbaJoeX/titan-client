from __future__ import division
#-
# ===========================================================================
# Copyright 2020 Autodesk, Inc.  All rights reserved.
#
# Use of this software is subject to the terms of the Autodesk license
# agreement provided at the time of installation or download, or which
# otherwise accompanies this software in either electronic or hard copy form.
# ===========================================================================
#+

import sys
import maya.api.OpenMaya as om
import maya.api.OpenMayaRender as omr
import textwrap

# Produces dependency graph node depthCheckerboardShader
# This node is an example of a surface shader that colors objects based on the distance from the camera.
# This node will also show a checkerboard pattern when the viewport is in "textured" mode.
# The inputs for this node are can be found in the Maya UI on the Attribute Editor for the node.
# The output attribute of the node is called "outColor". It is a 3 float value that represents the resulting color produced by the node.
# To use this shader, create a depthCheckerboardShader with Shading Group or connect its output to a Shading Group's "SurfaceShader" attribute.


def maya_useNewAPI():
	"""
	The presence of this function tells Maya that the plugin produces, and
	expects to be passed, objects created using the Maya Python API 2.0.
	"""
	pass

##################################################################
## Plugin Depth Checkerboard Shader Class Declaration
##################################################################
class depthCheckerboardShader(om.MPxNode):
	# Id tag for use with binary file format
	id = om.MTypeId( 0x81167 )

	# Input attributes
	aColorNear = None
	aColorFar = None
	aNear = None
	aFar = None
	aScale = None
	aUVCoord = None
	aPointCamera = None

	# Output attributes
	aOutColor = None

	@staticmethod
	def creator():
		return depthCheckerboardShader()

	@staticmethod
	def initialize():
		nAttr = om.MFnNumericAttribute()

		# Create input attributes

		depthCheckerboardShader.aColorNear = nAttr.createColor("color", "c")
		nAttr.keyable = True
		nAttr.storable = True
		nAttr.readable = True
		nAttr.writable = True
		nAttr.default = (0.0, 1.0, 0.0)			# Green


		depthCheckerboardShader.aColorFar = nAttr.createColor("colorFar", "cf")
		nAttr.keyable = True
		nAttr.storable = True
		nAttr.readable = True
		nAttr.writable = True
		nAttr.default = (0.0, 0.0, 1.0)			# Blue

		depthCheckerboardShader.aNear = nAttr.create("near", "n", om.MFnNumericData.kFloat)
		nAttr.keyable = True
		nAttr.storable = True
		nAttr.readable = True
		nAttr.writable = True
		nAttr.setMin(0.0)
		nAttr.setSoftMax(1000.0)

		depthCheckerboardShader.aFar = nAttr.create("far", "f", om.MFnNumericData.kFloat)
		nAttr.keyable = True
		nAttr.storable = True
		nAttr.readable = True
		nAttr.writable = True
		nAttr.setMin(0.0)
		nAttr.setSoftMax(1000.0)
		nAttr.default = 2.0

		depthCheckerboardShader.aScale = nAttr.create("scale", "sc", om.MFnNumericData.kFloat)
		nAttr.keyable = True
		nAttr.storable = True
		nAttr.readable = True
		nAttr.writable = True
		nAttr.setMin(0.0)
		nAttr.setSoftMax(1000.0)
		nAttr.default = 2.0

		child1 = nAttr.create( "uCoord", "u", om.MFnNumericData.kFloat)
		child2 = nAttr.create( "vCoord", "v", om.MFnNumericData.kFloat)
		depthCheckerboardShader.aUVCoord = nAttr.create( "uvCoord", "uv", child1, child2)
		nAttr.keyable = True
		nAttr.storable = True
		nAttr.readable = True
		nAttr.writable = True
		nAttr.hidden = True

		depthCheckerboardShader.aPointCamera = nAttr.createPoint("pointCamera", "p")
		nAttr.keyable = True
		nAttr.storable = True
		nAttr.readable = True
		nAttr.writable = True
		nAttr.hidden = True

		# Create output attributes
		depthCheckerboardShader.aOutColor = nAttr.createColor("outColor", "oc")
		nAttr.keyable = False
		nAttr.storable = False
		nAttr.readable = True
		nAttr.writable = False

		om.MPxNode.addAttribute(depthCheckerboardShader.aColorNear)
		om.MPxNode.addAttribute(depthCheckerboardShader.aColorFar)
		om.MPxNode.addAttribute(depthCheckerboardShader.aNear)
		om.MPxNode.addAttribute(depthCheckerboardShader.aFar)
		om.MPxNode.addAttribute(depthCheckerboardShader.aScale)
		om.MPxNode.addAttribute(depthCheckerboardShader.aUVCoord)
		om.MPxNode.addAttribute(depthCheckerboardShader.aPointCamera)
		om.MPxNode.addAttribute(depthCheckerboardShader.aOutColor)

		om.MPxNode.attributeAffects(depthCheckerboardShader.aColorNear, depthCheckerboardShader.aOutColor)
		om.MPxNode.attributeAffects(depthCheckerboardShader.aColorFar, depthCheckerboardShader.aOutColor)
		om.MPxNode.attributeAffects(depthCheckerboardShader.aNear, depthCheckerboardShader.aOutColor)
		om.MPxNode.attributeAffects(depthCheckerboardShader.aFar, depthCheckerboardShader.aOutColor)
		om.MPxNode.attributeAffects(depthCheckerboardShader.aScale, depthCheckerboardShader.aOutColor)
		om.MPxNode.attributeAffects(depthCheckerboardShader.aUVCoord, depthCheckerboardShader.aOutColor)
		om.MPxNode.attributeAffects(depthCheckerboardShader.aPointCamera, depthCheckerboardShader.aOutColor)

	def __init__(self):
		om.MPxNode.__init__(self)

	def compute(self, plug, block):
		# outColor or individial R, G, B channel
		if (plug != depthCheckerboardShader.aOutColor) and (plug.parent() != depthCheckerboardShader.aOutColor):
			return None # Let the Maya parent class compute the plug

		# get sample surface shading parameters
		pCamera   = block.inputValue(depthCheckerboardShader.aPointCamera).asFloatVector()
		cNear     = block.inputValue(depthCheckerboardShader.aColorNear).asFloatVector()
		cFar      = block.inputValue(depthCheckerboardShader.aColorFar).asFloatVector()
		nearClip  = block.inputValue(depthCheckerboardShader.aNear).asFloat()
		farClip   = block.inputValue(depthCheckerboardShader.aFar).asFloat()
		scale     = block.inputValue(depthCheckerboardShader.aScale).asFloat()
		uv        = block.inputValue(depthCheckerboardShader.aUVCoord).asFloat2()

		# pCamera.z is negative
		ratio = 1.0
		dist = farClip - nearClip
		if dist != 0:
			ratio = (farClip + pCamera.z) / dist
		resultColor = cNear * ratio + cFar*(1.0 - ratio)

		# checkerboard effect:
		fractUVx = (uv[0] * scale) % 1
		fractUVy = (uv[1] * scale) % 1
		if (fractUVx > 0.5) == (fractUVy > 0.5):
			resultColor = om.MFloatVector(0.0,0.0,0.0)

		# set ouput color attribute
		outColorHandle = block.outputValue( depthCheckerboardShader.aOutColor )
		outColorHandle.setMFloatVector( resultColor )
		outColorHandle.setClean()

		# The plug has been computed successfully
		return self

	def postConstructor(self):
		pass

##################################################################
## Plugin Depth Checkerboard Shader Override Class Declaration
##################################################################
class depthCheckerboardShaderOverride(omr.MPxSurfaceShadingNodeOverride):
	@staticmethod
	def creator(obj):
		return depthCheckerboardShaderOverride(obj)

	def __init__(self, obj):
		omr.MPxSurfaceShadingNodeOverride.__init__(self, obj)

		# Register fragments with the manager if needed
		fragmentMgr = omr.MRenderer.getFragmentManager()
		if fragmentMgr != None:
			if not fragmentMgr.hasFragment("depthCheckerboardShaderPluginFragmentTextured"):
				fragmentBody  = textwrap.dedent("""
					<fragment uiName=\"depthCheckerboardShaderPluginFragmentTextured\" name=\"depthCheckerboardShaderPluginFragmentTextured\" type=\"plumbing\" class=\"ShadeFragment\" version=\"1.0\">
						<description><![CDATA[Depth shader fragment]]></description>
						<properties>
							<float name=\"depthValue\" />
							<float3 name=\"color\" />
							<float3 name=\"colorFar\" />
							<float name=\"near\" />
							<float name=\"far\" />
							<float name=\"scale\" />
							<float2 name=\"uvCoord\" semantic=\"mayaUvCoordSemantic\" flags=\"varyingInputParam\" />
						</properties>
						<values>
							<float name=\"depthValue\" value=\"0.0\" />
							<float3 name=\"color\" value=\"0.0,1.0,0.0\" />
							<float3 name=\"colorFar\" value=\"0.0,0.0,1.0\" />
							<float name=\"near\" value=\"0.0\" />
							<float name=\"far\" value=\"2.0\" />
							<float name=\"scale\" value=\"2.0\" />
						</values>
						<outputs>
							<float3 name=\"outColor\" />
						</outputs>
						<implementation>
							<implementation render=\"OGSRenderer\" language=\"Cg\" lang_version=\"2.1\">
								<function_name val=\"depthCheckerboardShaderPluginFragmentTextured\" />
								<source><![CDATA[
									float3 depthCheckerboardShaderPluginFragmentTextured(float depthValue, float3 cNear, float3 cFar, float nearClip, float farClip, float scale, float2 uvCoord) \n
									{ \n
										float ratio = (farClip + depthValue)/(farClip - nearClip); \n
										float2 scaledUV = uvCoord * scale; \n
										float3 outColor = cNear*ratio + cFar*(1.0f - ratio); \n
										if ((frac(scaledUV.x) > 0.5) == (frac(scaledUV.y) > 0.5)) { \n
											outColor = float3(1.0, 0.0, 0.0); \n
										} \n
										return outColor; \n
									} \n]]>
								</source>
							</implementation>
							<implementation render=\"OGSRenderer\" language=\"HLSL\" lang_version=\"11.0\">
								<function_name val=\"depthCheckerboardShaderPluginFragmentTextured\" />
								<source><![CDATA[
									float3 depthCheckerboardShaderPluginFragmentTextured(float depthValue, float3 cNear, float3 cFar, float nearClip, float farClip, float scale, float2 uvCoord) \n
									{ \n
										float ratio = (farClip + depthValue)/(farClip - nearClip); \n
										float2 scaledUV = uvCoord * scale; \n
										float3 outColor = cNear*ratio + cFar*(1.0f - ratio); \n
										if ((frac(scaledUV.x) > 0.5) == (frac(scaledUV.y) > 0.5)) { \n
											outColor = float3(1.0, 0.0, 0.0); \n
										} \n
										return outColor; \n
									} \n]]>
								</source>
							</implementation>
							<implementation render=\"OGSRenderer\" language=\"GLSL\" lang_version=\"3.0\">
								<function_name val=\"depthCheckerboardShaderPluginFragmentTextured\" />
								<source><![CDATA[
									vec3 depthCheckerboardShaderPluginFragmentTextured(float depthValue, vec3 cNear, vec3 cFar, float nearClip, float farClip, float scale, vec2 uvCoord) \n
									{ \n
										float ratio = (farClip + depthValue)/(farClip - nearClip); \n
										vec2 scaledUV = uvCoord * scale; \n
										vec3 outColor = cNear*ratio + cFar*(1.0f - ratio); \n
										if ((fract(scaledUV.x) > 0.5) == (fract(scaledUV.y) > 0.5)) { \n
											outColor = vec3(1.0, 0.0, 0.0); \n
										} \n
										return outColor; \n
									} \n]]>
								</source>
							</implementation>
						</implementation>
					</fragment>""")

				fragmentMgr.addShadeFragmentFromBuffer(fragmentBody.encode('utf-8'), False)

			if not fragmentMgr.hasFragment("depthCheckerboardShaderPluginFragmentUntextured"):
				fragmentBody  = textwrap.dedent("""
					<fragment uiName=\"depthCheckerboardShaderPluginFragmentUntextured\" name=\"depthCheckerboardShaderPluginFragmentUntextured\" type=\"plumbing\" class=\"ShadeFragment\" version=\"1.0\">
						<description><![CDATA[Depth shader fragment]]></description>
						<properties>
							<float name=\"depthValue\" />
							<float3 name=\"color\" />
							<float3 name=\"colorFar\" />
							<float name=\"near\" />
							<float name=\"far\" />
						</properties>
						<values>
							<float name=\"depthValue\" value=\"0.0\" />
							<float3 name=\"color\" value=\"0.0,1.0,0.0\" />
							<float3 name=\"colorFar\" value=\"0.0,0.0,1.0\" />
							<float name=\"near\" value=\"0.0\" />
							<float name=\"far\" value=\"2.0\" />
						</values>
						<outputs>
							<float3 name=\"outColor\" />
						</outputs>
						<implementation>
							<implementation render=\"OGSRenderer\" language=\"Cg\" lang_version=\"2.1\">
								<function_name val=\"depthCheckerboardShaderPluginFragmentUntextured\" />
								<source><![CDATA[
									float3 depthCheckerboardShaderPluginFragmentUntextured(float depthValue, float3 cNear, float3 cFar, float nearClip, float farClip) \n
									{ \n
										float ratio = (farClip + depthValue)/(farClip - nearClip); \n
										return cNear*ratio + cFar*(1.0f - ratio); \n
									} \n]]>
								</source>
							</implementation>
							<implementation render=\"OGSRenderer\" language=\"HLSL\" lang_version=\"11.0\">
								<function_name val=\"depthCheckerboardShaderPluginFragmentUntextured\" />
								<source><![CDATA[
									float3 depthCheckerboardShaderPluginFragmentUntextured(float depthValue, float3 cNear, float3 cFar, float nearClip, float farClip) \n
									{ \n
										float ratio = (farClip + depthValue)/(farClip - nearClip); \n
										return cNear*ratio + cFar*(1.0f - ratio); \n
									} \n]]>
								</source>
							</implementation>
							<implementation render=\"OGSRenderer\" language=\"GLSL\" lang_version=\"3.0\">
								<function_name val=\"depthCheckerboardShaderPluginFragmentUntextured\" />
								<source><![CDATA[
									vec3 depthCheckerboardShaderPluginFragmentUntextured(float depthValue, vec3 cNear, vec3 cFar, float nearClip, float farClip) \n
									{ \n
										float ratio = (farClip + depthValue)/(farClip - nearClip); \n
										return cNear*ratio + cFar*(1.0f - ratio); \n
									} \n]]>
								</source>
							</implementation>
						</implementation>
					</fragment>""")

				fragmentMgr.addShadeFragmentFromBuffer(fragmentBody.encode('utf-8'), False)

			if not fragmentMgr.hasFragment("depthCheckerboardShaderPluginInterpolantFragment"):
				vertexFragmentBody  = textwrap.dedent("""
					<fragment uiName=\"depthCheckerboardShaderPluginInterpolantFragment\" name=\"depthCheckerboardShaderPluginInterpolantFragment\" type=\"interpolant\" class=\"ShadeFragment\" version=\"1.0\">
						<description><![CDATA[Depth shader vertex fragment]]></description>
						<properties>
							<float3 name=\"Pm\" semantic=\"Pm\" flags=\"varyingInputParam\" />
							<float4x4 name=\"worldViewProj\" semantic=\"worldviewprojection\" />
						</properties>
						<values>
						</values>
						<outputs>
							<float name=\"outDepthValue\" ^1s/>
						</outputs>
						<implementation>
							<implementation render=\"OGSRenderer\" language=\"Cg\" lang_version=\"2.1\">
								<function_name val=\"depthCheckerboardShaderPluginInterpolantFragment\" />
								<source><![CDATA[
									float depthCheckerboardShaderPluginInterpolantFragment(float depthValue) \n
									{ \n
										return depthValue; \n
									} \n]]>
								</source>
								<vertex_source><![CDATA[
									float idepthCheckerboardShaderPluginInterpolantFragment(float3 Pm, float4x4 worldViewProj) \n
									{ \n
										float4 pCamera = mul(worldViewProj, float4(Pm, 1.0f)); \n
										return (pCamera.z - pCamera.w*2.0f); \n
									} \n]]>
								</vertex_source>
							</implementation>
							<implementation render=\"OGSRenderer\" language=\"HLSL\" lang_version=\"11.0\">
								<function_name val=\"depthCheckerboardShaderPluginInterpolantFragment\" />
								<source><![CDATA[
									float depthCheckerboardShaderPluginInterpolantFragment(float depthValue) \n
									{ \n
										return depthValue; \n
									} \n]]>
								</source>
								<vertex_source><![CDATA[
									float idepthCheckerboardShaderPluginInterpolantFragment(float3 Pm, float4x4 worldViewProj) \n
									{ \n
										float4 pCamera = mul(float4(Pm, 1.0f), worldViewProj); \n
										return (pCamera.z - pCamera.w*2.0f); \n
									} \n]]>
								</vertex_source>
							</implementation>
							<implementation render=\"OGSRenderer\" language=\"GLSL\" lang_version=\"3.0\">
								<function_name val=\"depthCheckerboardShaderPluginInterpolantFragment\" />
								<source><![CDATA[
									float depthCheckerboardShaderPluginInterpolantFragment(float depthValue) \n
									{ \n
										return depthValue; \n
									} \n]]>
								</source>
								<vertex_source><![CDATA[
									float idepthCheckerboardShaderPluginInterpolantFragment(vec3 Pm, mat4 worldViewProj) \n
									{ \n
										vec4 pCamera = worldViewProj * vec4(Pm, 1.0f); \n
										return (pCamera.z - pCamera.w*2.0f); \n
									} \n]]>
								</vertex_source>
							</implementation>
						</implementation>
					</fragment>""")

				# In DirectX, need to specify a semantic for the output of the vertex shader
				if omr.MRenderer.drawAPI() == omr.MRenderer.kDirectX11:
					vertexFragmentBody = vertexFragmentBody.replace("^1s", "semantic=\"extraDepth\" ")
				else:
					vertexFragmentBody = vertexFragmentBody.replace("^1s", " ")

				fragmentMgr.addShadeFragmentFromBuffer(vertexFragmentBody.encode('utf-8'), False)

			if not fragmentMgr.hasFragment("depthCheckerboardShaderPluginGraphTextured"):
				fragmentGraphBody  = textwrap.dedent("""
					<fragment_graph name=\"depthCheckerboardShaderPluginGraphTextured\" ref=\"depthCheckerboardShaderPluginGraphTextured\" class=\"FragmentGraph\" version=\"1.0\">
						<fragments>
								<fragment_ref name=\"depthCheckerboardShaderPluginFragmentTextured\" ref=\"depthCheckerboardShaderPluginFragmentTextured\" />
								<fragment_ref name=\"depthCheckerboardShaderPluginInterpolantFragment\" ref=\"depthCheckerboardShaderPluginInterpolantFragment\" />
						</fragments>
						<connections>
							<connect from=\"depthCheckerboardShaderPluginInterpolantFragment.outDepthValue\" to=\"depthCheckerboardShaderPluginFragmentTextured.depthValue\" />
						</connections>
						<properties>
							<float3 name=\"Pm\" ref=\"depthCheckerboardShaderPluginInterpolantFragment.Pm\" semantic=\"Pm\" flags=\"varyingInputParam\" />
							<float4x4 name=\"worldViewProj\" ref=\"depthCheckerboardShaderPluginInterpolantFragment.worldViewProj\" semantic=\"worldviewprojection\" />
							<float3 name=\"color\" ref=\"depthCheckerboardShaderPluginFragmentTextured.color\" />
							<float3 name=\"colorFar\" ref=\"depthCheckerboardShaderPluginFragmentTextured.colorFar\" />
							<float name=\"near\" ref=\"depthCheckerboardShaderPluginFragmentTextured.near\" />
							<float name=\"far\" ref=\"depthCheckerboardShaderPluginFragmentTextured.far\" />
							<float name=\"scale\" ref=\"depthCheckerboardShaderPluginFragmentTextured.scale\" />
							<float2 name=\"uvCoord\" ref=\"depthCheckerboardShaderPluginFragmentTextured.uvCoord\" semantic=\"mayaUvCoordSemantic\" flags=\"varyingInputParam\" />
						</properties>
						<values>
							<float3 name=\"color\" value=\"0.0,1.0,0.0\" />
							<float3 name=\"colorFar\" value=\"0.0,0.0,1.0\" />
							<float name=\"near\" value=\"0.0\" />
							<float name=\"far\" value=\"2.0\" />
							<float name=\"scale\" value=\"2.0\" />
						</values>
						<outputs>
							<float3 name=\"outColor\" ref=\"depthCheckerboardShaderPluginFragmentTextured.outColor\" />
						</outputs>
					</fragment_graph>""")

				fragmentMgr.addFragmentGraphFromBuffer(fragmentGraphBody.encode('utf-8'))

			if not fragmentMgr.hasFragment("depthCheckerboardShaderPluginGraphUntextured"):
				fragmentGraphBody  = textwrap.dedent("""
					<fragment_graph name=\"depthCheckerboardShaderPluginGraphUntextured\" ref=\"depthCheckerboardShaderPluginGraphUntextured\" class=\"FragmentGraph\" version=\"1.0\">
						<fragments>
								<fragment_ref name=\"depthCheckerboardShaderPluginFragmentUntextured\" ref=\"depthCheckerboardShaderPluginFragmentUntextured\" />
								<fragment_ref name=\"depthCheckerboardShaderPluginInterpolantFragment\" ref=\"depthCheckerboardShaderPluginInterpolantFragment\" />
						</fragments>
						<connections>
							<connect from=\"depthCheckerboardShaderPluginInterpolantFragment.outDepthValue\" to=\"depthCheckerboardShaderPluginFragmentUntextured.depthValue\" />
						</connections>
						<properties>
							<float3 name=\"Pm\" ref=\"depthCheckerboardShaderPluginInterpolantFragment.Pm\" semantic=\"Pm\" flags=\"varyingInputParam\" />
							<float4x4 name=\"worldViewProj\" ref=\"depthCheckerboardShaderPluginInterpolantFragment.worldViewProj\" semantic=\"worldviewprojection\" />
							<float3 name=\"color\" ref=\"depthCheckerboardShaderPluginFragmentUntextured.color\" />
							<float3 name=\"colorFar\" ref=\"depthCheckerboardShaderPluginFragmentUntextured.colorFar\" />
							<float name=\"near\" ref=\"depthCheckerboardShaderPluginFragmentUntextured.near\" />
							<float name=\"far\" ref=\"depthCheckerboardShaderPluginFragmentUntextured.far\" />
						</properties>
						<values>
							<float3 name=\"color\" value=\"0.0,1.0,0.0\" />
							<float3 name=\"colorFar\" value=\"0.0,0.0,1.0\" />
							<float name=\"near\" value=\"0.0\" />
							<float name=\"far\" value=\"2.0\" />
						</values>
						<outputs>
							<float3 name=\"outColor\" ref=\"depthCheckerboardShaderPluginFragmentUntextured.outColor\" />
						</outputs>
					</fragment_graph>""")

				fragmentMgr.addFragmentGraphFromBuffer(fragmentGraphBody.encode('utf-8'))

	@staticmethod
	def supportedDrawAPIs():
		return omr.MRenderer.kOpenGL | omr.MRenderer.kOpenGLCoreProfile | omr.MRenderer.kDirectX11

	@staticmethod
	def fragmentName(isTexturedShading):
		if isTexturedShading:
			return "depthCheckerboardShaderPluginGraphTextured"
		return "depthCheckerboardShaderPluginGraphUntextured"

##
## Plugin setup
#######################################################
sRegistrantId = "depthCheckerboardShaderPlugin_py"

def initializePlugin(obj):
	plugin = om.MFnPlugin(obj, "Autodesk", "4.5", "Any")
	try:
		userClassify = "shader/surface:drawdb/shader/surface/depthCheckerboardShader_py"
		plugin.registerNode("depthCheckerboardShader_py", depthCheckerboardShader.id, depthCheckerboardShader.creator, depthCheckerboardShader.initialize, om.MPxNode.kDependNode, userClassify)
	except:
		sys.stderr.write("Failed to register node\n")
		raise

	try:
		global sRegistrantId
		omr.MDrawRegistry.registerSurfaceShadingNodeOverrideCreator("drawdb/shader/surface/depthCheckerboardShader_py", sRegistrantId, depthCheckerboardShaderOverride.creator)
	except:
		sys.stderr.write("Failed to register override\n")
		raise

def uninitializePlugin(obj):
	plugin = om.MFnPlugin(obj)
	try:
		plugin.deregisterNode(depthCheckerboardShader.id)
	except:
		sys.stderr.write("Failed to deregister node\n")
		raise

	try:
		global sRegistrantId
		omr.MDrawRegistry.deregisterSurfaceShadingNodeOverrideCreator("drawdb/shader/surface/depthCheckerboardShader_py", sRegistrantId)
	except:
		sys.stderr.write("Failed to deregister override\n")
		raise

