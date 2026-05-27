using System.Collections.Generic;
using System.Linq;
using System.Text;
using Unity.Collections;
using UnityEngine;
using UnityEngine.Rendering;

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

    public int stepsPerEval = 0;

    public bool toggleElevationLines = false;
    public bool updateThresholds = true;
    public List<ColourThreshold> colourThresholdsBedrock;
    public List<ColourThreshold> colourThresholdsWater;

    // States:
    // - Land Layer -
    // Bedrock height
    // Water depth
    // Transported Sediment amount
    // 
    // - Air Layers -
    // Air amount
    // Water amount
    // 
    int airLayers = 1;
    int stateVariables { get { return 3 + airLayers * 2; } set { } }

    // Start is called once before the first execution of Update after the MonoBehaviour is created
    void Start()
    {
        visualiser = new TerrainVisualiser(visualisationShader, visualisationMaterial);
        simulator = new TerrainSimulator(simulationShader);

        SetGridDimensions(new Vector2Int(2048, 2048));
        octaves = 5;
        cellSize = 25;
        seed = Vector4.zero;
    }

    // Update is called once per frame
    void Update()
    {
        if (toggleElevationLines)
        {
            visualiser.ToggleElevationLines();
            toggleElevationLines = false;
            if (steps == targetSteps) visualiser.GenerateVisTexture(simulator.states, threadGroups);
        }
        if (updateThresholds)
        {
            visualiser.SetThresholds(colourThresholdsBedrock, colourThresholdsWater);
            updateThresholds = false;
            if(steps == targetSteps) visualiser.GenerateVisTexture(simulator.states, threadGroups);
        }
        if (genSeed)
        {
            simulator.GenerateSeedTexture(octaves, cellSize, seed);
            if (targetSteps == 0) visualiser.GenerateVisTexture(simulator.states, threadGroups);
            genSeed = false;
            steps = 0;
            Evaluate();
        }
        if (restartSim)
        {
            simulator.GenerateSeedTexture(octaves, cellSize, seed);
            if(targetSteps == 0) visualiser.GenerateVisTexture(simulator.states, threadGroups);
            restartSim = false;
            steps = 0;
            Evaluate();
        }
        if (steps < targetSteps)
        {
            simulator.RunSimulationStep(threadGroups);
            visualiser.GenerateVisTexture(simulator.states, threadGroups);

            if((stepsPerEval > 0 && steps % stepsPerEval == 0) || steps == targetSteps - 1)
            {
                Evaluate();
            }

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

    public void Evaluate()
    {
        AsyncGPUReadbackRequest evalRequest = AsyncGPUReadback.Request(simulator.states, 0, AsyncEvaluation);
    }

    void AsyncEvaluation(AsyncGPUReadbackRequest request)
    {
        float[] sums = new float[stateVariables];
        for (int z = 0; z < stateVariables; z++)
        {
            sums[z] = 0;
        }

        float earth = 0;
        float water = 0;
        float air = 0;

        for (int z = 0; z < stateVariables; z++)
        {
            NativeArray<float> states = request.GetData<float>(z);
            sums[z] = states.Sum();

            if (z == 0 || z == 2) earth += sums[z];
            else if (z == 1 || z % 2 == 0) water += sums[z];
            else air += sums[z];
        }


        StringBuilder sb = new StringBuilder();
        for (int z = 0; z < stateVariables; z++)
        {
            if (z != 0) sb.Append(" ");
            sb.Append(sums[z]);
        }
        sb.AppendFormat(" | Earth: {0} | Water: {1} | Air: {2}", earth, water, air);
        Debug.Log(sb.ToString());
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
        simulator.Init(this.gridDimensions, stateVariables, airLayers);

        genSeed = true; 
    }

    Vector2Int FixGridDimensions(Vector2Int gridDimensions)
    {
        gridDimensions.x -= gridDimensions.x % 8;
        gridDimensions.y -= gridDimensions.y % 8;
        int numPoints = gridDimensions.x * gridDimensions.y;
        int bytesPerPoint = 4 * 4 + 2 * stateVariables * 4 /*+ (visualiser.mesh ? 8 * 4 : 0)*/;
        //int numTris = 2 * (gridDimensions.x - 1) * (gridDimensions.y - 1);
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
