using System;
using UnityEngine;

public class TerrainSimulator : MonoBehaviour
{
    [SerializeField]
    Vector2Int gridDimensions = new Vector2Int(256, 256);
    [SerializeField] uint octaves = 5;
    [SerializeField] float cellSize = 25;
    [SerializeField] int seed = 0;

    // Simulation Parameters:
    [SerializeField] float rain = 2; // amount of rain per pixel
    [SerializeField] float waterErosionExponent = 0.2f; // water erosion exponent
    [SerializeField] float erosionSpeed = 0.1f; // erosion speed
    [SerializeField] float creepSpeed = 1; // creep speed
    [SerializeField] float landscapeErosionStopProportion = 0.1f; // landscape erosion proportion
    [SerializeField] float uplift = 0; // uplift
    [SerializeField] float timestepLength = 1; // timestep

    [SerializeReference] ComputeShader simulator;
    [SerializeReference] ComputeShader visualiser;
    [SerializeReference] Material targetMat;
    [SerializeField] bool mesh = false;

    bool readA = true;
    RenderTexture texA;
    RenderTexture texB;
    RenderTexture texVis;

    int generationKernel = -1;
    int visualisationTextureKernel = -1;
    int simulationKernel = -1;

    Mesh[] meshes;
    RenderParams meshRenderParams;

    System.Random rng;

    // Start is called once before the first execution of Update after the MonoBehaviour is created
    void Start()
    {
        FixGridDimensions();

        rng = (seed == 0 ? new() : new(seed));

        InitTextures();

        InitShaderParams();

        GenerateSeedTexture();

    }

    private void OnDestroy()
    {
        texA.Release();
        texB.Release();
        texVis.Release();
    }

    // Update is called once per frame
    void Update()
    {
        RunSimulationStep();
        GenerateVisTexture();
        readA = !readA;
    }

    void GenerateSeedTexture()
    {
        if (generationKernel == -1)
        {
            generationKernel = simulator.FindKernel("Generate");
        }

        simulator.SetTexture(generationKernel, "genResult", readA ? texA : texB);
        simulator.Dispatch(generationKernel, gridDimensions.x / 8, gridDimensions.y / 8, 1);
    }

    void GenerateVisTexture()
    {
        if (visualisationTextureKernel == -1)
        {
            visualisationTextureKernel = visualiser.FindKernel("Tex");
            visualiser.SetTexture(visualisationTextureKernel, "visTexture", texVis);
        }

        visualiser.SetTexture(visualisationTextureKernel, "stateTexture", readA ? texA : texB);
        visualiser.Dispatch(visualisationTextureKernel, gridDimensions.x / 8, gridDimensions.y / 8, 1);
    }

    void RunSimulationStep()
    {
        if (simulationKernel == -1)
        {
            simulationKernel = simulator.FindKernel("Simulate");
        }

        simulator.SetTexture(simulationKernel, "input", readA ? texA : texB);
        simulator.SetTexture(simulationKernel, "result", readA ? texB : texA);
        simulator.Dispatch(simulationKernel, gridDimensions.x / 8, gridDimensions.y / 8, 1);
    }

    void FixGridDimensions()
    {
        gridDimensions.x -= gridDimensions.x % 8;
        gridDimensions.y -= gridDimensions.y % 8;
        int numPoints = gridDimensions.x * gridDimensions.y;
        int numTextures = 3;
        int bytesPerPoint = numTextures * 4 * 4 + (mesh ? 8 * 4 : 0);
        int numTris = 2 * (gridDimensions.x - 1) * (gridDimensions.y - 1);
        int bytesPerTri = 3 * 4;
        float roughMemoryUse = (float)numPoints * (float)bytesPerPoint + (float)(mesh ? numTris * bytesPerTri : 0);
        if (roughMemoryUse >= ((float)int.MaxValue) * 2)
        {
            Debug.LogWarning("Grid will use roughly " + roughMemoryUse / 1000000000 + "GB of graphics memory. Shrinking.");

            Vector2 prevGridSize = gridDimensions;

            float aspectRatio = gridDimensions.y / gridDimensions.x;
            float memoryUse = ((float)int.MaxValue) * 2;
            if (mesh)
            {
                float height = (bytesPerTri + aspectRatio * bytesPerTri + Mathf.Sqrt(bytesPerTri * bytesPerTri * (1 + aspectRatio * aspectRatio - 2 * aspectRatio) + aspectRatio * (2 * bytesPerTri * (memoryUse - bytesPerPoint) + bytesPerPoint * memoryUse))) / (bytesPerPoint + 2 * bytesPerTri);
                float width = height / aspectRatio;

                int gridHeight = (int)height;
                int gridWidth = (int)width;

                gridDimensions.x = gridWidth - gridWidth % 8;
                gridDimensions.y = gridHeight - gridHeight % 8;
            }
            else
            {
                float height = Mathf.Sqrt((memoryUse * aspectRatio) / bytesPerPoint);
                float width = height / aspectRatio;

                int gridHeight = (int)height;
                int gridWidth = (int)width;

                gridDimensions.x = gridWidth - gridWidth % 8;
                gridDimensions.y = gridHeight - gridHeight % 8;
            }

            Debug.LogWarning("Gridsize changed from" + prevGridSize + " to " + gridDimensions);
        }
        else
        {
            if (roughMemoryUse > 1000000000) Debug.Log("Grid will use roughly " + roughMemoryUse / 1000000000 + "GB of graphics memory");
            else if (roughMemoryUse > 1000000) Debug.Log("Grid will use roughly " + roughMemoryUse / 1000000 + "MB of graphics memory");
        }
        if (mesh)
        {
            gridDimensions.x -= gridDimensions.x % 128;
            gridDimensions.y -= gridDimensions.y % 128;
        }
    }

    void InitTextures()
    {
        texA = new RenderTexture(gridDimensions.x, gridDimensions.y, 0, RenderTextureFormat.ARGBFloat, RenderTextureReadWrite.Linear);
        texA.enableRandomWrite = true;
        texA.Create();
        texB = new RenderTexture(gridDimensions.x, gridDimensions.y, 0, RenderTextureFormat.ARGBFloat, RenderTextureReadWrite.Linear);
        texB.enableRandomWrite = true;
        texB.Create();
        texVis = new RenderTexture(gridDimensions.x, gridDimensions.y, 0, RenderTextureFormat.ARGBFloat, RenderTextureReadWrite.sRGB);
        texVis.enableRandomWrite = true;
        texVis.Create();
        texVis.wrapMode = TextureWrapMode.Repeat;
        targetMat.SetTexture("_MainTex", texVis);
    }

    void InitShaderParams()
    {
        int[] dim = { gridDimensions.x, gridDimensions.y };
        // Global variables
        simulator.SetInts("dim", dim);
        simulator.SetFloat("cellSizeSqr", cellSize * cellSize); 
        visualiser.SetInts("dim", dim);

        // Simulation parameters
        simulator.SetFloat("r", rain);
        simulator.SetFloat("m", waterErosionExponent);
        simulator.SetFloat("e", erosionSpeed);
        simulator.SetFloat("c", creepSpeed);
        simulator.SetFloat("p", landscapeErosionStopProportion);
        simulator.SetFloat("u", uplift);
        simulator.SetFloat("dt", timestepLength);

        float left = -((float)gridDimensions.x - 1) / 2;
        float bottom = -((float)gridDimensions.y - 1) / 2;

        // Noise parameters
        simulator.SetInt("octaves", (int)octaves);
        simulator.SetFloat("noiseScale", 2500 / cellSize);
        simulator.SetFloat("width", gridDimensions.x - 1);
        simulator.SetFloat("height", gridDimensions.y - 1);
        simulator.SetFloat("left", left + (float)rng.Next(Int16.MaxValue));
        simulator.SetFloat("bottom", bottom + (float)rng.Next(Int16.MaxValue));
    }
}
