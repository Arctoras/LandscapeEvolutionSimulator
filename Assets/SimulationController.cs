using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using TMPro;
using Unity.Collections;
using UnityEngine;
using UnityEngine.Rendering;
using static UnityEngine.Rendering.DebugUI;

public class SimulationController : MonoBehaviour
{
    TerrainSimulator simulator;
    TerrainVisualiser visualiser;

    [SerializeReference] ComputeShader simulationShader;
    [SerializeReference] ComputeShader visualisationShader;
    [SerializeReference] Material visualisationMaterial;

    [SerializeReference] TextMeshProUGUI stepCount;
    [SerializeReference] TextMeshProUGUI stepCountRate;

    Vector2Int gridDimensions;
    Vector3Int threadGroups = new Vector3Int(1, 1, 1);
    private uint octaves;
    public uint Octaves { get { return octaves; } set { octaves = value; genSeed = true; } }
    private float gridWidth;
    public float GridWidth { get { return gridWidth; } set { gridWidth = value; genSeed = true; } }
    private Vector4 seed;
    public Vector4 Seed { get { return seed; } set { seed = value; genSeed = true; } }

    bool genSeed = false;
    bool restartSim = false;
    int steps = 0;
    int targetSteps = 50000;
    float startTime = 0;

    public bool debugLogEvals = true;
    public string outputFile = "";
    public int stepsPerEval = 0;

    public bool toggleElevationLines = false;
    public bool updateThresholds = true;
    public List<ColourThreshold> colourThresholdsBedrock;
    public List<ColourThreshold> colourThresholdsWater;
    public List<ColourThreshold> colourThresholdsHumidity;

    // States:
    // - Land Layer -
    // Bedrock height
    // Water depth
    // Transported Sediment amount
    // 
    // - Air Layers -
    // Air amount
    // Water amount
    // Temperature
    // Relative Humidity (Not a proper state variable but to avoid recalculating during visualisation)
    // 
    int airLayers = 1;
    int stateVariables { get { return 3 + airLayers * 4; } set { } }

    List<float[]> evalResults = new List<float[]>();
    List<int> evalSteps = new List<int>();
    List<float> timeTaken = new List<float>();
    List<float> readTime = new List<float>();
    List<float> evalTime = new List<float>();

    public bool waterColumnDemo = false;
    public bool airColumnDemo = false;
    public Vector2 windVelocity = Vector2.zero;

    // Start is called once before the first execution of Update after the MonoBehaviour is created
    void Start()
    {
        visualiser = new TerrainVisualiser(visualisationShader, visualisationMaterial);
        simulator = new TerrainSimulator(simulationShader, windVelocity);

        SetGridDimensions(new Vector2Int(2048, 2048));
        octaves = 15;
        GridWidth = 10;
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
            visualiser.SetThresholds(colourThresholdsBedrock, colourThresholdsWater, colourThresholdsHumidity);
            updateThresholds = false;
            if(steps == targetSteps) visualiser.GenerateVisTexture(simulator.states, threadGroups);
        }
        if (genSeed || restartSim)
        {
            genSeed = false;
            restartSim = false;

            simulator.GenerateSeedTexture(octaves, 1000 * gridWidth / gridDimensions.x, seed);
            if (targetSteps == 0) visualiser.GenerateVisTexture(simulator.states, threadGroups);
            steps = 0;
            stepCount.text = steps.ToString();
            evalResults.Clear();
            evalSteps.Clear();
            timeTaken.Clear();
            Evaluate();
        }
        if (steps < targetSteps)
        {
            simulator.RunSimulationStep(threadGroups);
            visualiser.GenerateVisTexture(simulator.states, threadGroups);

            steps++;

            if((stepsPerEval > 0 && steps % stepsPerEval == 0) || steps == targetSteps)
            {
                Evaluate();
            }

            stepCount.text = steps.ToString();
            stepCountRate.text = Mathf.RoundToInt(1f / Time.unscaledDeltaTime).ToString() + "/s";
        } else
        {
            stepCountRate.text = "0/s";
        }
    }

    public void OnDestroy()
    {
        simulator.OnDestroy();
        visualiser.OnDestroy();

        if(outputFile != "")
        {
            StreamWriter results = File.CreateText("results/" + outputFile + ".csv");
            results.WriteLine("Steps,Eval Request Time,Eval Read Time,Eval Duration,Bedrock Height,Water Depth,Sediment Amount,Air Amount,Water Vapour Amount,Temperature,Humidity,Total Earth,Total Water,Total Air,Average Temperature");

            StringBuilder sb = new StringBuilder();
            for (int i = 0; i < evalResults.Count; i++)
            {
                sb.Append(evalSteps[i]).Append(",").Append(timeTaken[i]).Append(",").Append(readTime[i]).Append(",").Append(evalTime[i]);
                for (int state = 0; state < stateVariables + 4; state++)
                {
                    sb.Append(",");
                    sb.Append(evalResults[i][state]);
                }
                results.WriteLine(sb.ToString());
                sb.Clear();
            }

            results.Flush();
            results.Close();
        }
    }

    public void RestartSimulation()
    {
        restartSim = true;
    }

    public void Evaluate()
    {
        if (stepsPerEval < 0) return;

        evalSteps.Add(steps);
        float now = Time.realtimeSinceStartup;
        if (startTime == 0) startTime = now;
        timeTaken.Add(now - startTime);
        AsyncGPUReadback.Request(simulator.states, 0, AsyncEvaluation);

        //if(steps <= 50000 || steps % 50000 == 0)
        //{
        //    Texture2D vis = new Texture2D(gridDimensions.x, gridDimensions.y, TextureFormat.RGBA32, false);
        //    RenderTexture prev = RenderTexture.active;
        //    RenderTexture.active = visualiser.texVis;
        //    vis.ReadPixels(new Rect(0, 0, gridDimensions.x, gridDimensions.y), 0, 0);
        //    vis.Apply();
        //    RenderTexture.active = prev;
        //    byte[] bytes = vis.EncodeToPNG();
        //    File.WriteAllBytes("results/vis" + steps.ToString() + ".png", bytes);
        //}
    }

    void AsyncEvaluation(AsyncGPUReadbackRequest request)
    {
        float start = Time.realtimeSinceStartup;
        readTime.Add(start - startTime - timeTaken.Last());

        float cellArea = Mathf.Pow(1000 * gridWidth / gridDimensions.x, 2);

        float[] sums = new float[stateVariables];
        for (int state = 0; state < stateVariables; state++)
        {
            sums[state] = 0;
        }

        float earth = 0;
        float water = 0;
        float air = 0;
        float temperature = 0;
        int total = gridDimensions.x * gridDimensions.y;

        evalResults.Add(new float[stateVariables+4]);
        for (int state = 0; state < stateVariables; state++)
        {
            NativeArray<float> states = request.GetData<float>(state);
            sums[state] = states.Sum();

            if (state == 0 || state == 2) earth += sums[state];
            else if (state == 1) water += sums[state] * cellArea;
            else if (state % 4 == 3) air += sums[state];
            else if (state % 4 == 0) water += sums[state];
            else if (state % 4 == 1) temperature += sums[state] / total;
            evalResults.Last()[state] = sums[state];
        }
        evalResults.Last()[stateVariables] = earth;
        evalResults.Last()[stateVariables + 1] = water;
        evalResults.Last()[stateVariables + 2] = air;
        evalResults.Last()[stateVariables + 3] = temperature;

        float duration = Time.realtimeSinceStartup - start;
        evalTime.Add(duration);

        if (debugLogEvals)
        {
            // Volume is only guaranteed to be correct while no surface heights reach above 6km (Which should be always, but isn't when numerical errors are occuring)
            float volume = cellArea * (6000.0f * total - (sums[0] + sums[1]));
            float vapourPressure = 461520 * temperature * sums[4] / volume;
            float pressure = vapourPressure + temperature * 287.052874f * sums[3] / volume;
            float enhancement = 1.00071f * Mathf.Exp(0.000000045f * pressure);
            float saturationPressure = enhancement * Mathf.Exp(34.494f - 4924.99f / (temperature - 36.05f)) / Mathf.Pow(temperature - 168.15f, 1.57f);
            Debug.LogFormat("Pressure: {0} | Vapour Pressure: {1} | Saturation Pressure: {2} | Humidity: {3}", pressure, vapourPressure, saturationPressure, vapourPressure / saturationPressure);

            StringBuilder sb = new StringBuilder();
            for (int state = 0; state < stateVariables; state++)
            {
                if (state != 0) sb.Append(" ");
                sb.Append(sums[state]);
            }
            sb.AppendFormat(" | Earth: {0} | Water: {1} | Air: {2} | Temperature (Average): {3} | Eval Duration: {4}s", earth, water, air, temperature, duration);
            Debug.Log(sb.ToString());
        }
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
