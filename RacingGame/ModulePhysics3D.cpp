#include "Globals.h"
#include "Application.h"
#include "ModulePhysics3D.h"
#include "PhysBody3D.h"
#include "Primitive.h"

#include "glut/glut.h"

#ifdef _DEBUG
	#pragma comment (lib, "Bullet/libx86/BulletDynamics_debug.lib")
	#pragma comment (lib, "Bullet/libx86/BulletCollision_debug.lib")
	#pragma comment (lib, "Bullet/libx86/LinearMath_debug.lib")
#else
	#pragma comment (lib, "Bullet/libx86/BulletDynamics.lib")
	#pragma comment (lib, "Bullet/libx86/BulletCollision.lib")
	#pragma comment (lib, "Bullet/libx86/LinearMath.lib")
#endif

ModulePhysics3D::ModulePhysics3D(bool start_enabled) : Module(start_enabled), world(nullptr)
{
	collision_conf = new btDefaultCollisionConfiguration();
	dispatcher = new btCollisionDispatcher(collision_conf);
	broad_phase = new btDbvtBroadphase();
	solver = new btSequentialImpulseConstraintSolver();
	debug_draw = new DebugDrawer();
}

// Destructor
ModulePhysics3D::~ModulePhysics3D()
{
	delete debug_draw;
	delete solver;
	delete broad_phase;
	delete dispatcher;
	delete collision_conf;
}

// Render not available yet----------------------------------
bool ModulePhysics3D::Init()
{
	LOG("Creating 3D Physics simulation");
	bool ret = true;

	return ret;
}

// ---------------------------------------------------------
bool ModulePhysics3D::Start()
{
	LOG("Creating Physics environment");

	world = new btDiscreteDynamicsWorld(dispatcher, broad_phase, solver, collision_conf);
	world->setDebugDrawer(debug_draw);
	world->setGravity(GRAVITY);
	vehicle_raycaster = new btDefaultVehicleRaycaster(world);

	// Big rectangle as ground
	{
		btCollisionShape* colShape = new btBoxShape(btVector3(200.0f, 2.0f, 200.0f));

		mat4x4 glMatrix = IdentityMatrix;
		glMatrix.translate(0.f, -2.f, 0.f);
		btTransform startTransform;
		startTransform.setFromOpenGLMatrix(&glMatrix);

		btDefaultMotionState* myMotionState = new btDefaultMotionState(startTransform);
		btRigidBody::btRigidBodyConstructionInfo rbInfo(0.0f, myMotionState, colShape);

		btRigidBody* body = new btRigidBody(rbInfo);
		world->addRigidBody(body);
	}

	return true;
}

// ---------------------------------------------------------
update_status ModulePhysics3D::PreUpdate(float dt)
{
	world->stepSimulation(dt, 15);

	for (int n = 0; n < world->getDispatcher()->getNumManifolds(); n++)
	{
		btPersistentManifold* manifold = world->getDispatcher()->getManifoldByIndexInternal(n);
		if (manifold->getNumContacts() > 0)
		{
			PhysBody3D* body1 = (PhysBody3D*)manifold->getBody0()->getUserPointer();
			PhysBody3D* body2 = (PhysBody3D*)manifold->getBody1()->getUserPointer();

			if (body1 != nullptr && body2 != nullptr)
			{
				for (uint n = 0; n < body1->collision_listeners.Count(); n++)
				{
					body1->collision_listeners[n]->OnCollision(body1, body2);
				}

				for (uint n = 0; n < body2->collision_listeners.Count(); n++)
				{
					body2->collision_listeners[n]->OnCollision(body2, body1);
				}
			}
		}
	}

	return UPDATE_CONTINUE;
}

// ---------------------------------------------------------
update_status ModulePhysics3D::Update(float dt)
{
	if (App->debug == true)
	{
		glDisable(GL_LIGHTING);
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		world->debugDrawWorld();
		glEnable(GL_LIGHTING);		
	}

	return UPDATE_CONTINUE;
}

// ---------------------------------------------------------
update_status ModulePhysics3D::PostUpdate(float dt)
{
	return UPDATE_CONTINUE;
}

// Called before quitting
bool ModulePhysics3D::CleanUp()
{
	LOG("Destroying 3D Physics simulation");

	// Remove from the world all collision bodies
	for(int i = world->getNumCollisionObjects() - 1; i >= 0; i--)
	{
		btCollisionObject* obj = world->getCollisionObjectArray()[i];
		world->removeCollisionObject(obj);
	}
	
	for (p2List_item<btTypedConstraint*>*  item = constraints.getFirst(); item != nullptr; item = item->next)
	{
		//world->removeConstraint(item->data);
		//constraints.del(item);
	}

	delete world;

	return true;
}

PhysBody3D * ModulePhysics3D::RayCast(const vec3 & Origin, const vec3 & Direction, vec3& HitPoint)
{
	//TODO: NEW CODE
	//A snippet of new code that may be useful for you. Nothing to do here really
	vec3 Dir = normalize(Direction);

	btVector3 Start = btVector3(Origin.x, Origin.y, Origin.z);
	btVector3 End = btVector3(Origin.x + Dir.x * 1000.f, Origin.y + Dir.y * 1000.f, Origin.z + Dir.z * 1000.f);

	btCollisionWorld::ClosestRayResultCallback RayCallback(Start, End);

	// Perform raycast
	world->rayTest(Start, End, RayCallback);
	if (RayCallback.hasHit()) {

		HitPoint = vec3(RayCallback.m_hitPointWorld.x(), RayCallback.m_hitPointWorld.y(), RayCallback.m_hitPointWorld.z());
		return (PhysBody3D*)RayCallback.m_collisionObject->getUserPointer();
	}
	return nullptr;
}

void ModulePhysics3D::AddBodyToWorld(btRigidBody * body)
{
	world->addRigidBody(body);
}

void ModulePhysics3D::RemoveBodyFromWorld(btRigidBody * body)
{
	world->removeRigidBody(body);
}

// =============================================
void DebugDrawer::drawLine(const btVector3& from, const btVector3& to, const btVector3& color)
{
	line.origin.Set(from.getX(), from.getY(), from.getZ());
	line.destination.Set(to.getX(), to.getY(), to.getZ());
	line.color.Set(color.getX(), color.getY(), color.getZ());
	line.Render();
}

void DebugDrawer::drawContactPoint(const btVector3& PointOnB, const btVector3& normalOnB, btScalar distance, int lifeTime, const btVector3& color)
{
	point.transform.translate(PointOnB.getX(), PointOnB.getY(), PointOnB.getZ());
	point.color.Set(color.getX(), color.getY(), color.getZ());
	point.Render();
}

void DebugDrawer::reportErrorWarning(const char* warningString)
{
	LOG("Bullet warning: %s", warningString);
}

void DebugDrawer::draw3dText(const btVector3& location, const char* textString)
{
	LOG("Bullet draw text: %s", textString);
}

void DebugDrawer::setDebugMode(int debugMode)
{
	mode = (DebugDrawModes) debugMode;
}

int	 DebugDrawer::getDebugMode() const
{
	return mode;
}


btPoint2PointConstraint* ModulePhysics3D::AddConstraintP2P(const Primitive& bodyA, const Primitive& bodyB, const btVector3& pivotInA, const btVector3& pivotInB) {
	btPoint2PointConstraint* constraint = new btPoint2PointConstraint(*bodyA.body.GetBody(), *bodyB.body.GetBody(), pivotInA, pivotInB);
	world->addConstraint(constraint);
	constraints.add((btTypedConstraint*) constraint);
	return constraint;

}

btHingeConstraint* ModulePhysics3D::AddConstraintHinge(const Primitive& bodyA, const Primitive& bodyB, const btVector3& pivotInA, const btVector3& pivotInB, btVector3& axisInA, btVector3& axisInB) {
	btHingeConstraint* constraint = new btHingeConstraint(*bodyA.body.GetBody(), *bodyB.body.GetBody(), pivotInA, pivotInB, axisInA, axisInB);
	world->addConstraint(constraint);
	constraints.add((btTypedConstraint*)constraint);
	return constraint;
}

btSliderConstraint* ModulePhysics3D::AddConstraintSlider(const Primitive& bodyA, const Primitive& bodyB, btTransform& frameinA, btTransform& frameinB) {
	btSliderConstraint* constraint = new btSliderConstraint(*bodyA.body.GetBody(), *bodyB.body.GetBody(), frameinA, frameinB, true);
	world->addConstraint(constraint);
	constraints.add((btTypedConstraint*)constraint);
	return constraint;
}


btGeneric6DofConstraint* ModulePhysics3D::AddGeneric6DofConstraint(const Primitive& bodyA, const Primitive& bodyB, const btTransform& frameInA, const btTransform& frameInB, bool useLinearReferenceFrameA) {
	btGeneric6DofConstraint* constraint = new btGeneric6DofConstraint(*bodyA.body.GetBody(), *bodyB.body.GetBody(), frameInA, frameInB, useLinearReferenceFrameA);
	world->addConstraint(constraint);
	constraints.add((btTypedConstraint*)constraint);
	return constraint;
}

PhysVehicle3D* ModulePhysics3D::AddVehicle(const VehicleInfo& info)
{
	btCompoundShape* comShape = new btCompoundShape();
	shapes.add(comShape);

	btCollisionShape* chassisShape = new btBoxShape(btVector3(info.chassis_size.x * 0.5f, info.chassis_size.y * 0.5f, info.chassis_size.z * 0.5f));
	shapes.add(chassisShape);
	btTransform chassisTrans;
	chassisTrans.setIdentity();
	chassisTrans.setOrigin(btVector3(info.chassis_offset.x, info.chassis_offset.y, info.chassis_offset.z));
	comShape->addChildShape(chassisTrans, chassisShape);

	btCollisionShape* trunkShape = new btBoxShape(btVector3(info.trunk_size.x * 0.5f, info.trunk_size.y * 0.5f, info.trunk_size.z * 0.5f));
	shapes.add(trunkShape);
	btTransform trunkTrans;
	trunkTrans.setIdentity();
	trunkTrans.setOrigin(btVector3(info.trunk_offset.x, info.trunk_offset.y, info.trunk_offset.z));
	comShape->addChildShape(trunkTrans, trunkShape);

	btCollisionShape* cabinShape = new btBoxShape(btVector3(info.cabin_size.x * 0.5f, info.cabin_size.y * 0.5f, info.cabin_size.z * 0.5f));
	shapes.add(cabinShape);
	btTransform cabinTrans;
	cabinTrans.setIdentity();
	cabinTrans.setOrigin(btVector3(info.cabin_offset.x, info.cabin_offset.y, info.cabin_offset.z));
	comShape->addChildShape(cabinTrans, cabinShape);

	btTransform startTransform;
	startTransform.setIdentity();

	btVector3 localInertia(0, 0, 0);
	comShape->calculateLocalInertia(info.mass, localInertia);

	btDefaultMotionState* myMotionState = new btDefaultMotionState(startTransform);
	btRigidBody::btRigidBodyConstructionInfo rbInfo(info.mass, myMotionState, comShape, localInertia);

	btRigidBody* body = new btRigidBody(rbInfo);
	body->setContactProcessingThreshold(BT_LARGE_FLOAT);
	body->setActivationState(DISABLE_DEACTIVATION);

	world->addRigidBody(body);

	btRaycastVehicle::btVehicleTuning tuning;
	tuning.m_frictionSlip = info.frictionSlip;
	tuning.m_maxSuspensionForce = info.maxSuspensionForce;
	tuning.m_maxSuspensionTravelCm = info.maxSuspensionTravelCm;
	tuning.m_suspensionCompression = info.suspensionCompression;
	tuning.m_suspensionDamping = info.suspensionDamping;
	tuning.m_suspensionStiffness = info.suspensionStiffness;

	btRaycastVehicle* vehicle = new btRaycastVehicle(tuning, body, vehicle_raycaster);

	vehicle->setCoordinateSystem(0, 1, 2);

	for (int i = 0; i < info.num_wheels; ++i)
	{
		btVector3 conn(info.wheels[i].connection.x, info.wheels[i].connection.y, info.wheels[i].connection.z);
		btVector3 dir(info.wheels[i].direction.x, info.wheels[i].direction.y, info.wheels[i].direction.z);
		btVector3 axis(info.wheels[i].axis.x, info.wheels[i].axis.y, info.wheels[i].axis.z);

		vehicle->addWheel(conn, dir, axis, info.wheels[i].suspensionRestLength, info.wheels[i].radius, tuning, info.wheels[i].front);
	}

	// ---------------------

	PhysVehicle3D* pvehicle = new PhysVehicle3D(body, vehicle, info);
	world->addVehicle(vehicle);

	return pvehicle;
}