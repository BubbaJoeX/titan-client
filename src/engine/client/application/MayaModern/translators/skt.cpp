/**
 * This class handles importing and exporting a Skeleton (.skt) file
 */

#include "skt.h"

#include "Iff.h"
#include "Globals.h"
#include "Misc.h"
#include "Quaternion.h"
#include "Vector.h"

#include <maya/MGlobal.h>
#include <maya/MStatus.h>
#include <maya/MString.h>
#include <maya/MStringArray.h>
#include <maya/MPxFileTranslator.h>
#include <maya/MFnIkJoint.h>
#include <maya/MIntArray.h>
#include <maya/MFloatVectorArray.h>
#include <maya/MFnDagNode.h>
#include <maya/MObject.h>
#include <maya/MEulerRotation.h>

#include <vector>

// creates an instance of the SktTranslator
void* SktTranslator::creator()
{
    return new SktTranslator();
}

/**
 * Function that handles reading in the file (import/open operations)
 *
 * @param file the file
 * @param options options
 * @param mode file access mode
 * @return the operation success/failure
 */
MStatus SktTranslator::reader (const MFileObject& file, const MString& options, MPxFileTranslator::FileAccessMode mode)
{
    const char* fileName = file.expandedFullName().asChar();
    if(!Iff::isValid(fileName))
    {
        FATAL(true, ("%s could not be read as a valid IFF file!", fileName));
        return MS::kFailure;
    }
    Iff iff;
    if(iff.open(fileName, false))
    {
        if(iff.getRawDataSize() < 1)
        {
            FATAL(true, ("%s was read in as an IFF but its size was empty!", fileName));
            return MS::kFailure;
        }

        // a skt file can start with FORM SLOD, meaning it's a skeleton with multiple level of details
        // or, it can start directly with FORM SKTM, meaning there's only 1 level of detail for the skeleton,
        // so, we have to check what form the IFF file starts with to determine how to handle it;
        

        
        
        std::cerr << "starting SKT IMPORT" << std::endl;

        int16 lodCount = 0;

        // enter start of skeleton
        //iff.enterForm(TAG_SLOD);
        {
            
            // enter version tag (0000 is only version)
            //iff.enterForm(TAG_0000);
            {
                
                //iff.enterChunk(TAG_INFO);
                {
                    //lodCount = iff.read_int16();
                }
                //iff.exitChunk(TAG_INFO);

                char buffer[256];
                ConvertTagToString(iff.getCurrentName(), buffer);
                
                // for each level of detail
                //for(int s = 0; s < lodCount; s++)   //todo decide how to handle multiple level of detail imports on msh/mgn
                //{
                    int32 jointCount = 0;
                    MStringArray jointNames;
                    MIntArray jointParents;
                    std::vector<MQuaternion> preMultiplyRotations;
                    std::vector<MQuaternion> postMultiplyRotations;
                    MFloatVectorArray bindPoseJointTranslations;
                    std::vector<MQuaternion> bindPoseRotations;
                    MIntArray jointRotationOrder;

                    iff.enterForm(TAG_SKTM); // SKTM
                    {
                        
                        //todo this does not currently support versions 0 or 1
                        iff.enterForm(TAG_0002);
                        {
                            
                            
                            
                            iff.enterChunk(TAG_INFO);
                            {
                                jointCount = iff.read_int32();
                            }
                            iff.exitChunk(TAG_INFO);

                            //============= Joint Names =================
                            iff.enterChunk(TAG_NAME);
                            {
                                std::cerr << "INSIDE NAME INFO" << std::endl;
                                for(int i = 0; i < jointCount; i++)
                                {
                                    char jointName[MAX_PATH];
                                    iff.read_string(jointName, MAX_PATH-1);
                                    jointNames.append(jointName);
                                }
                            }
                            iff.exitChunk(TAG_NAME);

                            //============= Joint Parents =================
                            iff.enterChunk(TAG_PRNT);
                            {
                                for(int i = 0; i < jointCount; i++)
                                {
                                    jointParents.append(iff.read_int32());
                                }
                            }
                            iff.exitChunk(TAG_PRNT);

                            //============= Pre-Multiply Rotations =================
                            iff.enterChunk(TAG_RPRE);
                            {
                                for(int i = 0; i < jointCount; i++)
                                {
                                    Quaternion q = iff.read_floatQuaternion();
                                    MQuaternion mQ(q.x, q.y, q.z, q.w);
                                    preMultiplyRotations.emplace_back(mQ);
                                }
                            }
                            iff.exitChunk(TAG_RPRE);

                            //============= Post-Multiply Rotations =================
                            iff.enterChunk(TAG_RPST);
                            {
                                for(int i = 0; i < jointCount; i++)
                                {
                                    Quaternion q = iff.read_floatQuaternion();
                                    postMultiplyRotations.emplace_back(MQuaternion(q.x, q.y, q.z, q.w));
                                }
                            }
                            iff.exitChunk(TAG_RPST);

                            //============= Bind-Pose Joint Translation =================
                            iff.enterChunk(TAG_BPTR);
                            {
                                for(int i = 0; i < jointCount; i++)
                                {
                                    Vector v = iff.read_floatVector();
                                    bindPoseJointTranslations.append(MFloatVector(v.x, v.y, v.z));
                                }
                            }
                            iff.exitChunk(TAG_BPTR);

                            //============= Bind-Pose Rotations =================
                            iff.enterChunk(TAG_BPRO);
                            {
                                for(int i = 0; i < jointCount; i++)
                                {
                                    Quaternion q = iff.read_floatQuaternion();
                                    bindPoseRotations.emplace_back(MQuaternion(q.x, q.y, q.z, q.w));
                                }
                            }
                            iff.exitChunk(TAG_BPRO);

                            //============= Bind-Pose Joint Rotation Order =================
                            iff.enterChunk(TAG_JROR);
                            {
                                for(int i = 0; i < jointCount; i++)
                                {
                                    jointRotationOrder.append(iff.read_uint32()); // NOLINT(cppcoreguidelines-narrowing-conversions)
                                }
                            }
                            iff.exitChunk(TAG_JROR);
                        }
                        iff.exitForm(TAG_0002);
                        
                        // SWG pre-multiply rotation = Maya rotateOrientation
                        // SWG post-multiply rotation = Maya "orientation"
                        // SWG bind pose translation = Maya translation
                        // SWG bind pose rotation = Maya rotation
                        
                        MStatus status;
                        MFnIkJoint rootJoint;
                        rootJoint.create();
                        rootJoint.setRotateOrientation(getMayaEulerFromSwgQuaternion(preMultiplyRotations[0]).asQuaternion(), MSpace::kTransform, false);
                        rootJoint.setOrientation(getMayaEulerFromSwgQuaternion(postMultiplyRotations[0]));
                        rootJoint.setTranslation(bindPoseJointTranslations[0], MSpace::kTransform);
                        rootJoint.setRotation(getMayaEulerFromSwgQuaternion(bindPoseRotations[0]));
                        rootJoint.setName(jointNames[0]); // this should always be "root"
                        
                        std::vector<MObject> createdJoints;
                        createdJoints.emplace_back(rootJoint.object());
                        
                        // note: insertion this way only works because the skeleton's joints are
                        // always inserted such that its parents are created before we actually
                        // call the creation of the child
                        for(int i = 1; i < jointCount; i++)
                        {
                            MFnIkJoint joint;
                            joint.create(createdJoints[jointParents[i]]);
                            joint.setRotateOrientation(getMayaEulerFromSwgQuaternion(preMultiplyRotations[i]).asQuaternion(), MSpace::kTransform, false);
                            joint.setOrientation(getMayaEulerFromSwgQuaternion(postMultiplyRotations[i]));
                            joint.setTranslation(bindPoseJointTranslations[i], MSpace::kTransform);
                            joint.setRotation(getMayaEulerFromSwgQuaternion(bindPoseRotations[i]));
                            joint.setName(jointNames[i]);
                            createdJoints.emplace_back(joint.object());
                        }
                    }
                    iff.exitForm(TAG_SKTM);
                //} end for loop
            }
            iff.close();
        }

        return MS::kSuccess;

    }
    else
    {
        std::cerr << "fell out of IFF options, bad error! :(" << std::endl;
        return MS::kFailure;
    }

}

/**
 * @param q Quaternion from SWG SKT IFF
 * @return the inverse of MayaConversions::convertRotation which is used to
 *          to pack the rotation of a joint before it is saved as a SKT
 */
MEulerRotation SktTranslator::getMayaEulerFromSwgQuaternion(const MQuaternion& q)
{
    double sx = q.x;
    double sy = q.y;
    double sz = q.z;
    double sw = q.w;
    
    double x = atan2(2 * (sw * sx + sy * sz), 1 - 2 * (sx * sx + sy * sy));
    double y = asin(2 * (sw * sy - sz * sx));
    double z = atan2(2 * (sw * sz + sx * sy), 1 - 2 * (sy * sy + sz * sz));
    
    return {x, y, z, MEulerRotation::kXYZ};
}

/**
 * Handles writing out (exporting) the skeleton
 *
 * @param file the file to write
 * @param options the save options
 * @param mode the access mode of the file
 * @return the status of the operation
 */
MStatus SktTranslator::writer (const MFileObject& file, const MString& options, MPxFileTranslator::FileAccessMode mode)
{
    MString path = file.expandedFullName();
    MString cmd = "exportSkeleton -path \"" + path + "\"";
    return MGlobal::executeCommand(cmd);
}

/**
 * @return the file type this translator handles
 */
MString SktTranslator::defaultExtension () const
{
    return "skt";
}

MString SktTranslator::filter () const
{
    return "Skeleton Template | SWG (*.skt)";
}

/**
 * Validates if the provided file is one that this plug-in supports
 *
 * @param fileName the name of the file
 * @param buffer a buffer for reading into the file
 * @param size the size of the buffer
 * @return whether or not this file type is supported by this translator
 */
MPxFileTranslator::MFileKind SktTranslator::identifyFile(const MFileObject& fileName, const char* buffer, short size) const
{
    const char *name = fileName.resolvedName().asChar();
    int nameLength = (int)strlen(name);
    if((nameLength>4) && !strcasecmp(name+nameLength-4, ".skt"))
    {
        return (kIsMyFileType);
    }
    return (kNotMyFileType);
}

