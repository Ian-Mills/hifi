//
//  ModelOverlay.cpp
//
//
//  Created by Clement on 6/30/14.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include <Application.h>
#include <GlowEffect.h>

#include "ModelOverlay.h"

ModelOverlay::ModelOverlay()
    : _model(),
      _modelTextures(QVariantMap()),
      _scale(1.0f),
      _updateModel(false)
{
    _model.init();
    _isLoaded = false;
}

ModelOverlay::ModelOverlay(const ModelOverlay* modelOverlay) :
    Base3DOverlay(modelOverlay),
    _model(),
    _modelTextures(QVariantMap()),
    _url(modelOverlay->_url),
    _rotation(modelOverlay->_rotation),
    _scale(modelOverlay->_scale),
    _updateModel(false)
{
    _model.init();
    if (_url.isValid()) {
        _updateModel = true;
        _isLoaded = false;
    }
}

void ModelOverlay::update(float deltatime) {
    if (_updateModel) {
        _updateModel = false;
        
        _model.setSnapModelToCenter(true);
        _model.setRotation(_rotation);
        _model.setTranslation(_position);
        _model.setURL(_url);
        _model.simulate(deltatime, true);
    } else {
        _model.simulate(deltatime);
    }
    _isLoaded = _model.isActive();
}

bool ModelOverlay::addToScene(Overlay::Pointer overlay, std::shared_ptr<render::Scene> scene, render::PendingChanges& pendingChanges) {
    Base3DOverlay::addToScene(overlay, scene, pendingChanges);
    _model.addToScene(scene, pendingChanges);
    return true;
}

void ModelOverlay::removeFromScene(Overlay::Pointer overlay, std::shared_ptr<render::Scene> scene, render::PendingChanges& pendingChanges) {
    Base3DOverlay::removeFromScene(overlay, scene, pendingChanges);
    _model.removeFromScene(scene, pendingChanges);
}

void ModelOverlay::render(RenderArgs* args) {

    // check to see if when we added our model to the scene they were ready, if they were not ready, then
    // fix them up in the scene
    render::ScenePointer scene = Application::getInstance()->getMain3DScene();
    render::PendingChanges pendingChanges;
    if (_model.needsFixupInScene()) {
        _model.removeFromScene(scene, pendingChanges);
        _model.addToScene(scene, pendingChanges);
    }
    scene->enqueuePendingChanges(pendingChanges);

    if (!_visible) {
        return;
    }

    /*    
    if (_model.isActive()) {
        if (_model.isRenderable()) {
            float glowLevel = getGlowLevel();
            Glower* glower = NULL;
            if (glowLevel > 0.0f) {
                glower = new Glower(glowLevel);
            }
            _model.render(args, getAlpha());
            if (glower) {
                delete glower;
            }
        }
    }
    */
}

void ModelOverlay::setProperties(const QScriptValue &properties) {
    Base3DOverlay::setProperties(properties);
    
    QScriptValue urlValue = properties.property("url");
    if (urlValue.isValid()) {
        _url = urlValue.toVariant().toString();
        _updateModel = true;
        _isLoaded = false;
    }
    
    QScriptValue scaleValue = properties.property("scale");
    if (scaleValue.isValid()) {
        _scale = scaleValue.toVariant().toFloat();
        _model.setScaleToFit(true, _scale);
        _updateModel = true;
    }
    
    QScriptValue rotationValue = properties.property("rotation");
    if (rotationValue.isValid()) {
        QScriptValue x = rotationValue.property("x");
        QScriptValue y = rotationValue.property("y");
        QScriptValue z = rotationValue.property("z");
        QScriptValue w = rotationValue.property("w");
        if (x.isValid() && y.isValid() && z.isValid() && w.isValid()) {
            _rotation.x = x.toVariant().toFloat();
            _rotation.y = y.toVariant().toFloat();
            _rotation.z = z.toVariant().toFloat();
            _rotation.w = w.toVariant().toFloat();
        }
        _updateModel = true;
    }

    QScriptValue dimensionsValue = properties.property("dimensions");
    if (dimensionsValue.isValid()) {
        QScriptValue x = dimensionsValue.property("x");
        QScriptValue y = dimensionsValue.property("y");
        QScriptValue z = dimensionsValue.property("z");
        if (x.isValid() && y.isValid() && z.isValid()) {
            glm::vec3 dimensions;
            dimensions.x = x.toVariant().toFloat();
            dimensions.y = y.toVariant().toFloat();
            dimensions.z = z.toVariant().toFloat();
            _model.setScaleToFit(true, dimensions);
        }
        _updateModel = true;
    }
    
    QScriptValue texturesValue = properties.property("textures");
    if (texturesValue.isValid()) {
        QVariantMap textureMap = texturesValue.toVariant().toMap();
        foreach(const QString& key, textureMap.keys()) {
            
            QUrl newTextureURL = textureMap[key].toUrl();
            qDebug() << "Updating texture named" << key << "to texture at URL" << newTextureURL;
            
            QMetaObject::invokeMethod(&_model, "setTextureWithNameToURL", Qt::AutoConnection,
                                      Q_ARG(const QString&, key),
                                      Q_ARG(const QUrl&, newTextureURL));

            _modelTextures[key] = newTextureURL;  // Keep local track of textures for getProperty()
        }
    }

    if (properties.property("position").isValid()) {
        _updateModel = true;
    }
}

QScriptValue ModelOverlay::getProperty(const QString& property) {
    if (property == "url") {
        return _url.toString();
    }
    if (property == "scale") {
        return _scale;
    }
    if (property == "rotation") {
        return quatToScriptValue(_scriptEngine, _rotation);
    }
    if (property == "dimensions") {
        return vec3toScriptValue(_scriptEngine, _model.getScaleToFitDimensions());
    }
    if (property == "textures") {
        if (_modelTextures.size() > 0) {
            QScriptValue textures = _scriptEngine->newObject();
            foreach(const QString& key, _modelTextures.keys()) {
                textures.setProperty(key, _modelTextures[key].toString());
            }
            return textures;
        } else {
            return QScriptValue();
        }
    }

    return Base3DOverlay::getProperty(property);
}

bool ModelOverlay::findRayIntersection(const glm::vec3& origin, const glm::vec3& direction,
                                                        float& distance, BoxFace& face) {
    
    QString subMeshNameTemp;
    return _model.findRayIntersectionAgainstSubMeshes(origin, direction, distance, face, subMeshNameTemp);
}

bool ModelOverlay::findRayIntersectionExtraInfo(const glm::vec3& origin, const glm::vec3& direction,
                                                        float& distance, BoxFace& face, QString& extraInfo) {
    
    return _model.findRayIntersectionAgainstSubMeshes(origin, direction, distance, face, extraInfo);
}

ModelOverlay* ModelOverlay::createClone() const {
    return new ModelOverlay(this);
}
