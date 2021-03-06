#include "stdafx.h"
#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

#include "../../../tools/GeometryLib/frustum.h"
#include "../../../tools/GeometryLib/matrix44.h"


namespace InsideFrustum
{		
	TEST_CLASS(UnitTest1)
	{
	public:

		TEST_METHOD(Geometry_BoundingBoxInsideFrustum)
		{
			Frustum frustum;
			Matrix44 projection;

			//initialize the frustum
			projection.setPerspective(1.0, 0.75, 1.0, 10.0);
			frustum.fromCameraMatrix(projection);

			//start testing bounding boxes
			BoundingBox3D box;
			box= BoundingBox3D(Vector3D(0.0, 0.0, -1.0),Vector3D(1.0, 1.0, -2.0));
			Assert::IsTrue(frustum.isVisible(box));
			box = BoundingBox3D(Vector3D(-10.0, 0.0, 0.0), Vector3D(-9.0, 1.0, 1.0));
			Assert::IsFalse(frustum.isVisible(box));
		}
		TEST_METHOD(Geometry_BoundingBoxInsideFrustum2)
		{
			Frustum frustum;
			Matrix44 projection, view, cameraMatrix;

			//initialize the frustum
			projection.setPerspective(1.0, 0.75, 1.0, 10.0);
			view.setTranslation(Vector3D(-10.0, 0.0, 0.0));
			cameraMatrix = projection*view;
			frustum.fromCameraMatrix(cameraMatrix);

			//start testing bounding boxes
			BoundingBox3D box;
			box = BoundingBox3D(Vector3D(0.0, 0.0, -1.0), Vector3D(1.0, 1.0, -2.0));
			Assert::IsFalse(frustum.isVisible(box));
			box = BoundingBox3D(Vector3D(10.0, 0.0, 0.0), Vector3D(11.0, 1.0, 1.0));
			Assert::IsFalse(frustum.isVisible(box));
			box = BoundingBox3D(Vector3D(10.0, 0.0, -1.5), Vector3D(11.0, 1.0, -2.0));
			Assert::IsTrue(frustum.isVisible(box));
		}
		TEST_METHOD(Geometry_BoundingBoxInsideFrustum3)
		{
			Frustum frustum;
			Matrix44 projection, view, cameraMatrix;

			//initialize the frustum
			projection.setPerspective(1.0, 0.75, 1.0, 10.0);
			view.setTranslation(Vector3D(0.0, 0.0, -11.0));
			cameraMatrix = projection*view;
			frustum.fromCameraMatrix(cameraMatrix);

			//start testing bounding boxes
			BoundingBox3D box;
			box = BoundingBox3D(Vector3D(0.0, 0.0, -1.0), Vector3D(1.0, 1.0, -2.0));
			Assert::IsFalse(frustum.isVisible(box));
			box = BoundingBox3D(Vector3D(0.0, 0.0, 0.0), Vector3D(1.0, 1.0, 2.0));
			Assert::IsTrue(frustum.isVisible(box));
			box = BoundingBox3D(Vector3D(0.0, 0.0, -1.5), Vector3D(1.0, 1.0, -2.0));
			Assert::IsFalse(frustum.isVisible(box));
		}
		TEST_METHOD(Geometry_BoundingBoxInsideFrustum4)
		{
			Frustum frustum;
			Matrix44 projection, view, cameraMatrix;

			//initialize the frustum
			projection.setPerspective(1.0, 0.75, 1.0, 10.0);
			view.setRotation(Quaternion(-1.57,0,0).inverse());
			cameraMatrix = projection*view;
			frustum.fromCameraMatrix(cameraMatrix);

			//test bounding boxes
			BoundingBox3D box;
			Point3D point;
			Point3D rotatedPoint = cameraMatrix * Point3D(0,0,0);
			box = BoundingBox3D(Vector3D(12.0, 0.0, 0.0), Vector3D(13.0, 1.0, 1.0));
			Assert::IsFalse(frustum.isVisible(box));
			box = BoundingBox3D(Vector3D(5.0, 0.0, 0.0), Vector3D(6.0, 1.0, 2.0));
			Assert::IsTrue(frustum.isVisible(box));
			box = BoundingBox3D(Vector3D(0.0, 0.0, -1.5), Vector3D(0.3, 1.0, -2.0));
			Assert::IsFalse(frustum.isVisible(box));
			box = BoundingBox3D(Vector3D(-5.0, 0.0, -3.0), Vector3D(-2.0, 3.0, 6.0));
			Assert::IsFalse(frustum.isVisible(box));

			//transform + box
			Matrix44 translation, rot, transform;
			translation.setTranslation(Vector3D(5.0, 1.0, -1.0));
			rot.setRotation(Quaternion(1.0, 0.3, 0.7));
			transform = translation * rot;
			box= BoundingBox3D(Vector3D(0.0, 0.0, -1.5), Vector3D(0.3, 1.0, -2.0));
			Assert::IsTrue(frustum.isVisible(transform*box));


			//test points
			point = Point3D(5.0, 0.0, 0.0);
			Assert::IsTrue(frustum.isVisible(point));
			point = Point3D(2.0, 0.0, 0.5);
			Assert::IsTrue(frustum.isVisible(point));
			point = Point3D(10.0, 3.0, 0.0);
			Assert::IsTrue(frustum.isVisible(point));
			point = Point3D(15.0, 0.0, 0.0);
			Assert::IsFalse(frustum.isVisible(point));
			point = Point3D(5.0, 10.0, 0.0);
			Assert::IsFalse(frustum.isVisible(point));
			point = Point3D(5.0, 0.0, -10.0);
			Assert::IsFalse(frustum.isVisible(point));
			point = Point3D(-5.0, 0.0, 0.0);
			Assert::IsFalse(frustum.isVisible(point));

			//camera*modelview transform + box
			cameraMatrix = projection * view * transform;
			frustum.fromCameraMatrix(cameraMatrix);
			Assert::IsTrue(frustum.isVisible(box));
		}
		TEST_METHOD(Geometry_PointInsideFrustum)
		{
			Frustum frustum;
			Matrix44 cameraMatrix;

			//initialize the frustum: from z=-1 (near plane) to z=-10 (far plane)
			cameraMatrix.setPerspective(1.0, 0.75, 1.0, 10.0);
			frustum.fromCameraMatrix(cameraMatrix);

			//start testing bounding boxes
			Point3D point;
			point = Point3D(0.0, 0.0, -1.1);
			Assert::IsTrue(frustum.isVisible(point));
			point = Point3D(0.0, 0.0, -0.5);
			Assert::IsFalse(frustum.isVisible(point));
			point = Point3D(0.0, 0.0, -10.5);
			Assert::IsFalse(frustum.isVisible(point));
			point = Point3D(-2.0, 0.0, -1.5);
			Assert::IsFalse(frustum.isVisible(point));
		}
		TEST_METHOD(Geometry_PointInsideFrustum2)
		{
			Frustum frustum;
			Matrix44 perspective,view,cameraMatrix;

			//initialize the frustum: from z=-1 (near plane) to z=-10 (far plane)
			perspective.setPerspective(1.0, 0.75, 1.0, 10.0);
			view.setTranslation(Vector3D(-10.0, 0.0, 0.0));
			cameraMatrix = perspective * view;
			frustum.fromCameraMatrix(cameraMatrix);

			//start testing bounding boxes
			Point3D point;
			point = Point3D(10.0, 0.0, -1.1);
			Assert::IsTrue(frustum.isVisible(point));
			point = Point3D(10.0, 0.0, -0.5);
			Assert::IsFalse(frustum.isVisible(point));
			point = Point3D(10.0, 0.0, -10.5);
			Assert::IsFalse(frustum.isVisible(point));
			point = Point3D(-2.0, 0.0, -1.5);
			Assert::IsFalse(frustum.isVisible(point));
		}
	};
}