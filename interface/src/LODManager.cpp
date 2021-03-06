//
//  LODManager.cpp
//  interface/src/LODManager.h
//
//  Created by Clement on 1/16/15.
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include <SettingHandle.h>
#include <Util.h>

#include "Application.h"
#include "ui/DialogsManager.h"
#include "InterfaceLogging.h"

#include "LODManager.h"

Setting::Handle<float> desktopLODDecreaseFPS("desktopLODDecreaseFPS", DEFAULT_DESKTOP_LOD_DOWN_FPS);
Setting::Handle<float> hmdLODDecreaseFPS("hmdLODDecreaseFPS", DEFAULT_HMD_LOD_DOWN_FPS);

LODManager::LODManager() {
    calculateAvatarLODDistanceMultiplier();
}

float LODManager::getLODDecreaseFPS() {
    if (Application::getInstance()->isHMDMode()) {
        return getHMDLODDecreaseFPS();
    }
    return getDesktopLODDecreaseFPS();
}

float LODManager::getLODIncreaseFPS() {
    if (Application::getInstance()->isHMDMode()) {
        return getHMDLODIncreaseFPS();
    }
    return getDesktopLODIncreaseFPS();
}


void LODManager::autoAdjustLOD(float currentFPS) {
    
    // NOTE: our first ~100 samples at app startup are completely all over the place, and we don't
    // really want to count them in our average, so we will ignore the real frame rates and stuff
    // our moving average with simulated good data
    const int IGNORE_THESE_SAMPLES = 100;
    if (_fpsAverageUpWindow.getSampleCount() < IGNORE_THESE_SAMPLES) {
        currentFPS = ASSUMED_FPS;
        _lastStable = _lastUpShift = _lastDownShift = usecTimestampNow();
    }
    
    _fpsAverageStartWindow.updateAverage(currentFPS);
    _fpsAverageDownWindow.updateAverage(currentFPS);
    _fpsAverageUpWindow.updateAverage(currentFPS);
    
    quint64 now = usecTimestampNow();

    bool changed = false;
    quint64 elapsedSinceDownShift = now - _lastDownShift;
    quint64 elapsedSinceUpShift = now - _lastUpShift;

    quint64 lastStableOrUpshift = glm::max(_lastUpShift, _lastStable);
    quint64 elapsedSinceStableOrUpShift = now - lastStableOrUpshift;
    
    if (_automaticLODAdjust) {
    
        // LOD Downward adjustment 
        // If we've been downshifting, we watch a shorter downshift window so that we will quickly move toward our
        // target frame rate. But if we haven't just done a downshift (either because our last shift was an upshift,
        // or because we've just started out) then we look at a much longer window to consider whether or not to start
        // downshifting.
        bool doDownShift = false; 

        if (_isDownshifting) {
            // only consider things if our DOWN_SHIFT time has elapsed...
            if (elapsedSinceDownShift > DOWN_SHIFT_ELPASED) {
                doDownShift = _fpsAverageDownWindow.getAverage() < getLODDecreaseFPS();
                
                if (!doDownShift) {
                    qCDebug(interfaceapp) << "---- WE APPEAR TO BE DONE DOWN SHIFTING -----";
                    _isDownshifting = false;
                    _lastStable = now;
                }
            }
        } else {
            doDownShift = (elapsedSinceStableOrUpShift > START_SHIFT_ELPASED 
                                && _fpsAverageStartWindow.getAverage() < getLODDecreaseFPS());
        }
        
        if (doDownShift) {

            // Octree items... stepwise adjustment
            if (_octreeSizeScale > ADJUST_LOD_MIN_SIZE_SCALE) {
                _octreeSizeScale *= ADJUST_LOD_DOWN_BY;
                if (_octreeSizeScale < ADJUST_LOD_MIN_SIZE_SCALE) {
                    _octreeSizeScale = ADJUST_LOD_MIN_SIZE_SCALE;
                }
                changed = true;
            }

            if (changed) {
                if (_isDownshifting) {
                    // subsequent downshift
                    qCDebug(interfaceapp) << "adjusting LOD DOWN..."
                                << "average fps for last "<< DOWN_SHIFT_WINDOW_IN_SECS <<"seconds was " 
                                << _fpsAverageDownWindow.getAverage() 
                                << "minimum is:" << getLODDecreaseFPS() 
                                << "elapsedSinceDownShift:" << elapsedSinceDownShift
                                << " NEW _octreeSizeScale=" << _octreeSizeScale;
                } else {
                    // first downshift
                    qCDebug(interfaceapp) << "adjusting LOD DOWN after initial delay..."
                                << "average fps for last "<< START_DELAY_WINDOW_IN_SECS <<"seconds was " 
                                << _fpsAverageStartWindow.getAverage() 
                                << "minimum is:" << getLODDecreaseFPS() 
                                << "elapsedSinceUpShift:" << elapsedSinceUpShift
                                << " NEW _octreeSizeScale=" << _octreeSizeScale;
                }

                _lastDownShift = now;
                _isDownshifting = true;

                emit LODDecreased();
            }
        } else {
    
            // LOD Upward adjustment
            if (elapsedSinceUpShift > UP_SHIFT_ELPASED) {
            
                if (_fpsAverageUpWindow.getAverage() > getLODIncreaseFPS()) {

                    // Octee items... stepwise adjustment
                    if (_octreeSizeScale < ADJUST_LOD_MAX_SIZE_SCALE) {
                        if (_octreeSizeScale < ADJUST_LOD_MIN_SIZE_SCALE) {
                            _octreeSizeScale = ADJUST_LOD_MIN_SIZE_SCALE;
                        } else {
                            _octreeSizeScale *= ADJUST_LOD_UP_BY;
                        }
                        if (_octreeSizeScale > ADJUST_LOD_MAX_SIZE_SCALE) {
                            _octreeSizeScale = ADJUST_LOD_MAX_SIZE_SCALE;
                        }
                        changed = true;
                    }
                }
        
                if (changed) {
                    qCDebug(interfaceapp) << "adjusting LOD UP... average fps for last "<< UP_SHIFT_WINDOW_IN_SECS <<"seconds was " 
                                << _fpsAverageUpWindow.getAverage()
                                << "upshift point is:" << getLODIncreaseFPS() 
                                << "elapsedSinceUpShift:" << elapsedSinceUpShift
                                << " NEW _octreeSizeScale=" << _octreeSizeScale;

                    _lastUpShift = now;
                    _isDownshifting = false;

                    emit LODIncreased();
                }
            }
        }
    
        if (changed) {
            calculateAvatarLODDistanceMultiplier();
            _shouldRenderTableNeedsRebuilding = true;
            auto lodToolsDialog = DependencyManager::get<DialogsManager>()->getLodToolsDialog();
            if (lodToolsDialog) {
                lodToolsDialog->reloadSliders();
            }
        }
    }
}

void LODManager::resetLODAdjust() {
    _fpsAverageStartWindow.reset();
    _fpsAverageDownWindow.reset();
    _fpsAverageUpWindow.reset();
    _lastUpShift = _lastDownShift = usecTimestampNow();
    _isDownshifting = false;
}

QString LODManager::getLODFeedbackText() {
    // determine granularity feedback
    int boundaryLevelAdjust = getBoundaryLevelAdjust();
    QString granularityFeedback;
    
    switch (boundaryLevelAdjust) {
        case 0: {
            granularityFeedback = QString(".");
        } break;
        case 1: {
            granularityFeedback = QString(" at half of standard granularity.");
        } break;
        case 2: {
            granularityFeedback = QString(" at a third of standard granularity.");
        } break;
        default: {
            granularityFeedback = QString(" at 1/%1th of standard granularity.").arg(boundaryLevelAdjust + 1);
        } break;
    }
    
    // distance feedback
    float octreeSizeScale = getOctreeSizeScale();
    float relativeToDefault = octreeSizeScale / DEFAULT_OCTREE_SIZE_SCALE;
    int relativeToTwentyTwenty = 20 / relativeToDefault;

    QString result;
    if (relativeToDefault > 1.01) {
        result = QString("20:%1 or %2 times further than average vision%3").arg(relativeToTwentyTwenty).arg(relativeToDefault,0,'f',2).arg(granularityFeedback);
    } else if (relativeToDefault > 0.99) {
        result = QString("20:20 or the default distance for average vision%1").arg(granularityFeedback);
    } else if (relativeToDefault > 0.01) {
        result = QString("20:%1 or %2 of default distance for average vision%3").arg(relativeToTwentyTwenty).arg(relativeToDefault,0,'f',3).arg(granularityFeedback);
    } else {
        result = QString("%2 of default distance for average vision%3").arg(relativeToDefault,0,'f',3).arg(granularityFeedback);
    }
    return result;
}

bool LODManager::shouldRender(const RenderArgs* args, const AABox& bounds) {
    const float maxScale = (float)TREE_SCALE;
    const float octreeToMeshRatio = 4.0f; // must be this many times closer to a mesh than a voxel to see it.
    float octreeSizeScale = args->_sizeScale;
    int boundaryLevelAdjust = args->_boundaryLevelAdjust;
    float visibleDistanceAtMaxScale = boundaryDistanceForRenderLevel(boundaryLevelAdjust, octreeSizeScale) / octreeToMeshRatio;
    float distanceToCamera = glm::length(bounds.calcCenter() - args->_viewFrustum->getPosition());
    float largestDimension = bounds.getLargestDimension();
    
    static bool shouldRenderTableNeedsBuilding = true;
    static QMap<float, float> shouldRenderTable;
    if (shouldRenderTableNeedsBuilding) {
        
        float SMALLEST_SCALE_IN_TABLE = 0.001f; // 1mm is plenty small
        float scale = maxScale;
        float factor = 1.0f;
        
        while (scale > SMALLEST_SCALE_IN_TABLE) {
            scale /= 2.0f;
            factor /= 2.0f;
            shouldRenderTable[scale] = factor;
        }
        
        shouldRenderTableNeedsBuilding = false;
    }
    
    float closestScale = maxScale;
    float visibleDistanceAtClosestScale = visibleDistanceAtMaxScale;
    QMap<float, float>::const_iterator lowerBound = shouldRenderTable.lowerBound(largestDimension);
    if (lowerBound != shouldRenderTable.constEnd()) {
        closestScale = lowerBound.key();
        visibleDistanceAtClosestScale = visibleDistanceAtMaxScale * lowerBound.value();
    }
    
    if (closestScale < largestDimension) {
        visibleDistanceAtClosestScale *= 2.0f;
    }
    
    return distanceToCamera <= visibleDistanceAtClosestScale;
};

// TODO: This is essentially the same logic used to render octree cells, but since models are more detailed then octree cells
//       I've added a voxelToModelRatio that adjusts how much closer to a model you have to be to see it.
bool LODManager::shouldRenderMesh(float largestDimension, float distanceToCamera) {
    const float octreeToMeshRatio = 4.0f; // must be this many times closer to a mesh than a voxel to see it.
    float octreeSizeScale = getOctreeSizeScale();
    int boundaryLevelAdjust = getBoundaryLevelAdjust();
    float maxScale = (float)TREE_SCALE;
    float visibleDistanceAtMaxScale = boundaryDistanceForRenderLevel(boundaryLevelAdjust, octreeSizeScale) / octreeToMeshRatio;
    
    if (_shouldRenderTableNeedsRebuilding) {
        _shouldRenderTable.clear();
        
        float SMALLEST_SCALE_IN_TABLE = 0.001f; // 1mm is plenty small
        float scale = maxScale;
        float visibleDistanceAtScale = visibleDistanceAtMaxScale;
        
        while (scale > SMALLEST_SCALE_IN_TABLE) {
            scale /= 2.0f;
            visibleDistanceAtScale /= 2.0f;
            _shouldRenderTable[scale] = visibleDistanceAtScale;
        }
        _shouldRenderTableNeedsRebuilding = false;
    }
    
    float closestScale = maxScale;
    float visibleDistanceAtClosestScale = visibleDistanceAtMaxScale;
    QMap<float, float>::const_iterator lowerBound = _shouldRenderTable.lowerBound(largestDimension);
    if (lowerBound != _shouldRenderTable.constEnd()) {
        closestScale = lowerBound.key();
        visibleDistanceAtClosestScale = lowerBound.value();
    }
    
    if (closestScale < largestDimension) {
        visibleDistanceAtClosestScale *= 2.0f;
    }
    
    return (distanceToCamera <= visibleDistanceAtClosestScale);
}

void LODManager::setOctreeSizeScale(float sizeScale) {
    _octreeSizeScale = sizeScale;
    calculateAvatarLODDistanceMultiplier();
    _shouldRenderTableNeedsRebuilding = true;
}

void LODManager::calculateAvatarLODDistanceMultiplier() {
    _avatarLODDistanceMultiplier = AVATAR_TO_ENTITY_RATIO / (_octreeSizeScale / DEFAULT_OCTREE_SIZE_SCALE);
}

void LODManager::setBoundaryLevelAdjust(int boundaryLevelAdjust) {
    _boundaryLevelAdjust = boundaryLevelAdjust;
    _shouldRenderTableNeedsRebuilding = true;
}


void LODManager::loadSettings() {
    setDesktopLODDecreaseFPS(desktopLODDecreaseFPS.get());
    setHMDLODDecreaseFPS(hmdLODDecreaseFPS.get());
}

void LODManager::saveSettings() {
    desktopLODDecreaseFPS.set(getDesktopLODDecreaseFPS());
    hmdLODDecreaseFPS.set(getHMDLODDecreaseFPS());
}


