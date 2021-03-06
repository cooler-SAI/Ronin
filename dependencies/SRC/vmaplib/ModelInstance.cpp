/*
 * Copyright (C) 2005-2011 MaNGOS <http://getmangos.com/>
 * Copyright (C) 2008-2014 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2014-2017 Sandshroud <https://github.com/Sandshroud>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <g3dlite\G3D.h>
#include "VMapLib.h"
#include "VMapDefinitions.h"

using G3D::Vector3;
using G3D::Ray;

namespace VMAP
{
    ModelInstance::ModelInstance(const ModelSpawn &spawn, WorldModel* model): ModelSpawn(spawn), iModel(model)
    {
        iInvRot = G3D::Matrix3::fromEulerAnglesZYX(G3D::pi()*iRot.y/180.f, G3D::pi()*(iRot.x)/180.f, G3D::pi()*iRot.z/180.f).inverse();
        iModel->GetTreeBound(iModelBound);
        iInvScale = 1.f/iScale;
    }

    bool ModelInstance::intersectRay(const G3D::Ray& pRay, float& pMaxDist, bool pStopAtFirstHit) const
    {
        if (!iModel)
            return false;
        float time = pRay.intersectionTime(iBound);
        if (time == G3D::inf())
            return false;
        Vector3 p = iInvRot * (pRay.origin() - iPos) * iInvScale;
        Ray modRay(p, iInvRot * pRay.direction());
        float distance = pMaxDist * iInvScale;
        bool hit = iModel->IntersectRay(modRay, distance, pStopAtFirstHit);
        if (hit)
        {
            distance *= iScale;
            pMaxDist = distance;
        }
        return hit;
    }

    void ModelInstance::getWMOData(const G3D::Vector3& p, WMOData &data, G3D::int32 requiredFlags, G3D::int32 ignoredFlags) const
    {
        if (!iModel)
        {
            OUT_DEBUG("<object not loaded>");
            return;
        }

        // Our bounds or our submodel bound box
        if (!iBound.contains(p))
            return;

        // Rotate our position
        G3D::Vector3 offset = (iInvRot * (p - iPos)) * iInvScale;
        float zDist;
        if (flags & MOD_M2)
        {   // Only grab height data here
            if(iModel->ZCheck(offset, iModelBound, zDist, data))
            {
                Vector3 modelGround = offset + (G3D::Vector3(0.f, 0.f, -1.f) * zDist);
                // Transform back to world space. Note that:
                // Mat * vec == vec * Mat.transpose()
                // and for rotation matrices: Mat.inverse() == Mat.transpose()
                float world_Z = ((modelGround * iInvRot) * iScale + iPos).z;
                if (data.ground_CalcZ < world_Z)
                {
                    data.groundResult = true;
                    data.ground_CalcZ = world_Z + 0.1f;
                }
            }
            return;
        }

        if (iModel->WMOCheck(offset, iModelBound, zDist, data, requiredFlags, ignoredFlags))
        {
            Vector3 modelGround = offset + (G3D::Vector3(0.f, 0.f, -1.f) * zDist);
            // Transform back to world space. Note that:
            // Mat * vec == vec * Mat.transpose()
            // and for rotation matrices: Mat.inverse() == Mat.transpose()
            float world_Z = ((modelGround * iInvRot) * iScale + iPos).z;
            if (data.ground_CalcZ < world_Z)
            {
                data.groundResult = true;
                data.ground_CalcZ = world_Z + 0.1f;
            }

            if (data.offset_z < world_Z)
            {
                data.hitResult = true;
                data.adtId = adtId;
                data.hitInstance = this;
                data.offset_z = world_Z;
            }
        }
    }

    void ModelInstance::intersectPoint(const G3D::Vector3& p, AreaInfo &info) const
    {
        if (!iModel)
        {
            OUT_DEBUG("<object not loaded>");
            return;
        }

        // M2 files don't contain area info, only WMO files
        if (flags & MOD_M2)
            return;
        if (!iBound.contains(p))
            return;
        float zDist;
        G3D::Vector3 offset = (iInvRot * (p - iPos)) * iInvScale;
        if (iModel->IntersectPoint(p, iModelBound, zDist, info))
        {
            Vector3 modelGround = offset + (G3D::Vector3(0.f, 0.f, -1.f) * zDist);
            // Transform back to world space. Note that:
            // Mat * vec == vec * Mat.transpose()
            // and for rotation matrices: Mat.inverse() == Mat.transpose()
            float world_Z = ((modelGround * iInvRot) * iScale + iPos).z;
            if (info.ground_Z < world_Z)
            {
                info.ground_Z = world_Z;
                info.adtId = adtId;
            }
        }
    }

    bool ModelInstance::GetLocationInfo(const G3D::Vector3& p, LocationInfo &info) const
    {
        if (!iModel)
        {
            OUT_DEBUG("<object not loaded>");
            return false;
        }

        // M2 files don't contain area info, only WMO files
        if (flags & MOD_M2)
            return false;
        if (!iBound.contains(p))
            return false;
        float zDist;
        G3D::Vector3 offset = (iInvRot * (p - iPos)) * iInvScale;
        if (iModel->GetLocationInfo(offset, iModelBound, zDist, info))
        {
            Vector3 modelGround = offset + (G3D::Vector3(0.f, 0.f, -1.f) * zDist);
            // Transform back to world space. Note that:
            // Mat * vec == vec * Mat.transpose()
            // and for rotation matrices: Mat.inverse() == Mat.transpose()
            float world_Z = ((modelGround * iInvRot) * iScale + iPos).z;
            if (info.ground_Z < world_Z) // hm...could it be handled automatically with zDist at intersection?
            {
                info.ground_Z = world_Z;
                info.hitInstance = this;
                return true;
            }
        }
        return false;
    }

    bool ModelInstance::GetLiquidLevel(const G3D::Vector3& p, const VMAP::GroupModel *model, float &liqHeight) const
    {
        float zDist;
        G3D::Vector3 offset = (iInvRot * (p - iPos)) * iInvScale;
        if (model->GetLiquidLevel(offset, zDist))
        {
            // calculate world height (zDist in model coords):
            // assume WMO not tilted (wouldn't make much sense anyway)
            liqHeight = zDist * iScale + iPos.z;
            return true;
        }
        return false;
    }

    void ModelInstance::CalcOffsetDirection(const G3D::Vector3 pos, G3D::Vector3 &p, G3D::Vector3 &up) const
    {
        p = iInvRot * (pos - iPos) * iInvScale;
        up = iInvRot * Vector3(0.f, 0.f, 1.f);
    }

    GameobjectModelInstance::GameobjectModelInstance(const GameobjectModelSpawn &spawn, WorldModel* model, G3D::int32 m_phase) : iModel(model), m_PhaseMask(m_phase)
    {
        BoundBase = spawn.BoundBase;
        name = spawn.name;
    }

    void GameobjectModelInstance::SetData(float x, float y, float z, float orientation, float scale)
    {
        iPos = Vector3(x, y, z);
        iScale = scale;
        iInvScale = 1.0f / iScale;
        iOrientation = orientation;

        G3D::Matrix3 iRotation = G3D::Matrix3::fromEulerAnglesZYX(iOrientation, 0, 0);
        iInvRot = iRotation.inverse();

        // transform bounding box:
        G3D::AABox box = G3D::AABox(BoundBase.low() * iScale, BoundBase.high() * iScale), rotated_bounds;
        for (int i = 0; i < 8; ++i)
            rotated_bounds.merge(iRotation * box.corner(i));
        iBound = rotated_bounds + iPos;
    }

    bool GameobjectModelInstance::intersectRay(const G3D::Ray& ray, float& MaxDist, bool StopAtFirstHit, G3D::int32 ph_mask) const
    {
        if(m_PhaseMask != -1 && ph_mask != -1)
            if (!(m_PhaseMask & ph_mask))
                return false;
        float time = ray.intersectionTime(getBounds());
        if (time == G3D::inf())
            return false;

        // child bounds are defined in object space:
        Vector3 p = iInvRot * (ray.origin() - iPos) * iInvScale;
        Ray modRay(p, iInvRot * ray.direction());
        float distance = MaxDist * iInvScale;
        bool hit = iModel->IntersectRay(modRay, distance, StopAtFirstHit);
        if (hit)
        {
            distance *= iScale;
            MaxDist = distance;
        }
        return hit;
    }

}
