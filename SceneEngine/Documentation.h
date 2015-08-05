// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

/*!
    \brief World rendering technologies and lighting resolve

    SceneEngine provides rendering technologies and features for environments, as well
    as the structure for the lighting resolve step.

    It works on top of RenderCore (which provides the primitive low-level API
    for draw operations) to create the elements of the scene: forests of objects,
    terrain, lighting, fogging, water, etc.
    
    ## "Parsing" a scene
    SceneEngine provides the technologies for rendering a scene, but it doesn't provide 
    all of the structure for composing a scene. Many engines use a rigid structure (such
    as a scene graph) for composing and organising the part of the scene. But the nature
    of this structure is (in some ways) less generic than the parts themselves. So this
    behaviour has been pushed into a higher level library.

    Instead, the SceneEngine simply introduces the concept of "parsing" a scene. That just
    means moving through the structure of the scene (whatever structure that is) and executing
    the component parts as necessary.

    Here, we are using the term in the same way we use "walking" in "walking through a tree"
    or "parsing" in "parsing a scripting language." Let's imagine that the scene is (conceptually)
    a structured set of commands (such as draw this object, conditionally set this render state, etc) 
    which we must parse through every frame.

    ## Lighting "parser"
    We also have a lighting "parser", which works in parallel to the scene parser. Here, we
    again imagine the lighting process as a set of commands. The lighting parser controls
    how we step through those commands.

    The goal is to separate the contents of the scene (ie, what objects make up
    our world) and the lighting process (ie, how the objects are presented).

    The scene parser is responsible for what is in the scene; and the lighting parser is
    responsible for how it appears on screen.

    ## Key concepts
    Concept | Description
    ------- | -----------
    ISceneParser | interface for a scene implementation. Implemented at a higher level
    LightingParserContext | context state for the lighting parser
    ILIghtingParserPlugin | plug-in to allow extensions to the lighting process
    LightingParser_ExecuteScene() | main lighting parser entry point
    LightingResolveContext | context while performing deferred lighting resolve steps
    
    ## Scene element implementations
    Concept | Description
    ------- | -----------
    TerrainManager | top level manager for terrain rendering
    PlacementsManager | top level manager for "placements" (or simple static objects)
    Ocean_Execute() | deep ocean rendering
    ShallowSurfaceManager | shallow water surfaces (such as rivers and lakes)
    VolumetricFogManager | volumetric fog effect renderer
    AmbientOcclusion_Render() | ambient occlusion renderer
    TiledLighting_CalculateLighting() | tiled & cascaded lighting implementation
    ToneMap_Execute() | tonemap entry point

    ## Platform considerations

    SceneEngine is a combination of platform-generic and platform-specific functionality.
    Most of the platform specific code is pushed into the RenderCore library. However,
    some effects and technologies in SceneEngine are designed and optimised for specific
    hardware (or a specific platform). 
    
    As a result, while the core parts of SceneEngine are platform-generic, there are 
    some parts that must be disabled on some platforms (particularly for lower-power platforms).

    Given that the graphics API is selected at compile-time (as opposed to link-time or run-time),
    the are separate scene engine library outputs for each graphics API (in other words, 
    the library compiles to SceneEngineDX11.lib or SceneEngineOpenGLES.lib).

*/
namespace SceneEngine {}

