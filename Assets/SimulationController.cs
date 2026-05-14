using System.Collections.Generic;
using UnityEngine;

public class SimulationController : MonoBehaviour
{
    TerrainSimulator simulator;
    TerrainVisualiser visualiser;

    [SerializeReference] ComputeShader simulationShader;
    [SerializeReference] ComputeShader visualisationShader;
    [SerializeReference] Material visualisationMaterial;

    Vector2Int gridDimensions;
    Vector3Int threadGroups = new Vector3Int(1, 1, 1);
    private uint octaves;
    public uint Octaves { get { return octaves; } set { octaves = value; genSeed = true; } }
    private float cellSize;
    public float CellSize { get { return cellSize; } set { cellSize = value; genSeed = true; } }
    private Vector4 seed;
    public Vector4 Seed { get { return seed; } set { seed = value; genSeed = true; } }

    bool genSeed = false;
    bool restartSim = false;
    int steps = 0;
    int targetSteps = 0;
    int stepsPerRender = 1;

    public bool updateThresholds = true;
    public List<ColourThreshold> colourThresholdsBedrock;
    public List<ColourThreshold> colourThresholdsWater;

    // Start is called once before the first execution of Update after the MonoBehaviour is created
    void Start()
    {
        visualiser = new TerrainVisualiser(visualisationShader, visualisationMaterial);
        simulator = new TerrainSimulator(simulationShader);

        SetGridDimensions(new Vector2Int(1024, 1024));
        octaves = 5;
        cellSize = 25;
        seed = Vector4.zero;
    }

    // Update is called once per frame
    void Update()
    {
        if (updateThresholds)
        {
            visualiser.SetThresholds(colourThresholdsBedrock, colourThresholdsWater);
            updateThresholds = false;
        }
        if (genSeed)
        {
            simulator.GenerateSeedTexture(octaves, cellSize, seed);
            visualiser.GenerateVisTexture(simulator.states, threadGroups);
            genSeed = false;
        }
        if (restartSim)
        {
            simulator.GenerateSeedTexture(octaves, cellSize, seed);
            if(targetSteps == 0) visualiser.GenerateVisTexture(simulator.states, threadGroups);
            steps = 0;
            restartSim = false;
        }
        if (steps < targetSteps)
        {
            simulator.RunSimulationStep(threadGroups);
            if(steps % stepsPerRender == 0) visualiser.GenerateVisTexture(simulator.states, threadGroups);
            steps++;
        }
    }

    public void OnDestroy()
    {
        simulator.OnDestroy();
        visualiser.OnDestroy();
    }

    public void RestartSimulation()
    {
        restartSim = true;
    }
    public void SetTargetSteps(int newTarget)
    {
        if (steps > newTarget) RestartSimulation();
        targetSteps = newTarget;
    }

    public void SetGridDimensions(Vector2Int gridDimensions)
    {
        this.gridDimensions = FixGridDimensions(gridDimensions);
        threadGroups.x = this.gridDimensions.x / 8;
        threadGroups.y = this.gridDimensions.y / 8;

        visualiser.Init(this.gridDimensions);
        simulator.Init(this.gridDimensions);

        genSeed = true;
    }

    Vector2Int FixGridDimensions(Vector2Int gridDimensions)
    {
        gridDimensions.x -= gridDimensions.x % 8;
        gridDimensions.y -= gridDimensions.y % 8;
        int numPoints = gridDimensions.x * gridDimensions.y;
        int numTextures = 3;
        int bytesPerPoint = numTextures * 4 * 4 /*+ (visualiser.mesh ? 8 * 4 : 0)*/;
        int numTris = 2 * (gridDimensions.x - 1) * (gridDimensions.y - 1);
        //int bytesPerTri = 3 * 4;
        float roughMemoryUse = (float)numPoints * (float)bytesPerPoint /*+ (float)(visualiser.mesh ? numTris * bytesPerTri : 0)*/;
        if (roughMemoryUse >= 5000000000f) // Limit graphics memory usage to 5GB maximum
        {
            Debug.LogWarning("Grid will use roughly " + roughMemoryUse / 1000000000 + "GB of graphics memory. Shrinking.");

            Vector2 prevGridSize = gridDimensions;

            float aspectRatio = gridDimensions.y / gridDimensions.x;
            float memoryUse = 5000000000f;
            /*if (visualiser.mesh)
            {
                float height = (bytesPerTri + aspectRatio * bytesPerTri + Mathf.Sqrt(bytesPerTri * bytesPerTri * (1 + aspectRatio * aspectRatio - 2 * aspectRatio) + aspectRatio * (2 * bytesPerTri * (memoryUse - bytesPerPoint) + bytesPerPoint * memoryUse))) / (bytesPerPoint + 2 * bytesPerTri);
                float width = height / aspectRatio;

                int gridHeight = (int)height;
                int gridWidth = (int)width;

                gridDimensions.x = gridWidth - gridWidth % 8;
                gridDimensions.y = gridHeight - gridHeight % 8;
            }
            else*/
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
        /*if (visualiser.mesh)
        {
            gridDimensions.x -= gridDimensions.x % 128;
            gridDimensions.y -= gridDimensions.y % 128;
        }*/

        gridDimensions.x = Mathf.Max(gridDimensions.x, 8);
        gridDimensions.y = Mathf.Max(gridDimensions.y, 8);

        return gridDimensions;
    }
}
