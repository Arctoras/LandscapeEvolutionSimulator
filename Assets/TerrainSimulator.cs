using System;
using UnityEngine;
using UnityEngine.Windows;

public class TerrainSimulator
{
    ComputeShader simulator;

    bool readA = true;
    RenderTexture texA = null;
    RenderTexture texB = null;

    public Texture states { get { return readA ? texA : texB; } }

    int generationKernel = -1;
    int simulationKernel = -1;

    private float[] simulationParameters = new float[12];
    public float uplift { get { return simulationParameters[0]; } set { simulationParameters[0] = value; simulator.SetFloat("u", simulationParameters[0]); } }
    public float creepSpeed { get { return simulationParameters[1]; } set { simulationParameters[1] = value; simulator.SetFloat("c", simulationParameters[1]); } }
    public float rain { get { return simulationParameters[2]; } set { simulationParameters[2] = value; simulator.SetFloat("r", simulationParameters[2]); } } // amount of rain per pixel
    public float bedrockErosionSpeed { get { return simulationParameters[3]; } set { simulationParameters[3] = value; simulator.SetFloat("e0", simulationParameters[3]); } }
    public float regolithErosionSpeed { get { return simulationParameters[4]; } set { simulationParameters[4] = value; simulator.SetFloat("e1", simulationParameters[4]); } }
    public float waterErosionExponent { get { return simulationParameters[5]; } set { simulationParameters[5] = value; simulator.SetFloat("m", simulationParameters[5]); } } // water depth multiplier
    public float waterErosionExponent2 { get { return simulationParameters[6]; } set { simulationParameters[6] = value; simulator.SetFloat("n", simulationParameters[6]); } } // slope multiplier
    public float sedimentationRate { get { return simulationParameters[7]; } set { simulationParameters[7] = value; simulator.SetFloat("s", simulationParameters[7]); } }
    public float weatheringRate { get { return simulationParameters[8]; } set { simulationParameters[8] = value; simulator.SetFloat("w", simulationParameters[8]); } }
    public float regolithProtectionExponent { get { return simulationParameters[9]; } set { simulationParameters[9] = value; simulator.SetFloat("p0", simulationParameters[9]); } } // protecting against bedrock erosion (Cannot be 0)
    public float regolithProtectionExponent2 { get { return simulationParameters[10]; } set { simulationParameters[10] = value; simulator.SetFloat("p1", simulationParameters[10]); } } // protecting against bedrock weathering (Cannot be 0)  
    public float timestepLength { get { return simulationParameters[11]; } set { simulationParameters[11] = value; simulator.SetFloat("dt", simulationParameters[11]); } } // timestep

    public TerrainSimulator(ComputeShader simulator)
    {
        this.simulator = simulator;

        if (File.Exists("config/lastSimParams.data")) Load("config/lastSimParams.data");
        else if (File.Exists("config/defaultSimParams.data")) Load("config/defaultSimParams.data");

        timestepLength = 0.5f;
        weatheringRate = 0.01f;
        creepSpeed = 1;
        bedrockErosionSpeed = 0.1f;
        regolithErosionSpeed = 0.3f;
        regolithProtectionExponent = 2.0f;
        regolithProtectionExponent2 = 4.0f;
    }

    public void OnDestroy()
    {
        if(texA) texA.Release();
        if(texB) texB.Release();

        if (!Directory.Exists("config")) Directory.CreateDirectory("config");
        Save("config/lastSimParams.data");
    }

    public void GenerateSeedTexture(uint octaves, float cellSize, Vector4 seed)
    {
        int[] dim = { texA.width, texA.height };
        simulator.SetInts("dim", dim);
        simulator.SetFloat("cellSize", cellSize);
        simulator.SetFloat("doubleCellSize", cellSize * 2);

        float left = -((float)texA.width - 1) / 2;
        float bottom = -((float)texA.height - 1) / 2;

        simulator.SetInt("octaves", (int)octaves);
        simulator.SetFloat("noiseScale", 2500 / cellSize);
        simulator.SetFloat("width", texA.width - 1);
        simulator.SetFloat("height", texA.height - 1);
        simulator.SetFloat("left", left);
        simulator.SetFloat("bottom", bottom);

        float[] seedValues = { seed.x, seed.y, seed.z, seed.w };
        simulator.SetFloats("seed", seedValues);

        if (generationKernel == -1)
        {
            generationKernel = simulator.FindKernel("Generate");
        }

        simulator.SetTexture(generationKernel, "genResult", readA ? texA : texB);
        simulator.Dispatch(generationKernel, texA.width / 8, texA.height / 8, 1);

        rain = 0.1f / timestepLength;
    }

    public void RunSimulationStep(Vector3Int threadGroups)
    {
        if (simulationKernel == -1)
        {
            simulationKernel = simulator.FindKernel("Simulate");
        }

        simulator.SetTexture(simulationKernel, "input", readA ? texA : texB);
        simulator.SetTexture(simulationKernel, "result", readA ? texB : texA);

        simulator.Dispatch(simulationKernel, threadGroups.x, threadGroups.y, threadGroups.z);

        readA = !readA;

        rain = 0;
    }


    public void Init(Vector2Int gridDimensions)
    {
        if (texA != null)
        {
            texA.Release();
            texA.width = gridDimensions.x;
            texA.height = gridDimensions.y;
        }
        else
        {
            texA = new RenderTexture(gridDimensions.x, gridDimensions.y, 0, RenderTextureFormat.ARGBFloat, RenderTextureReadWrite.Linear);
            texA.enableRandomWrite = true;
        }
        if (texB != null)
        {
            texB.Release();
            texB.width = gridDimensions.x;
            texB.height = gridDimensions.y;
        }
        else
        {
            texB = new RenderTexture(gridDimensions.x, gridDimensions.y, 0, RenderTextureFormat.ARGBFloat, RenderTextureReadWrite.Linear);
            texB.enableRandomWrite = true;
        }

        texA.Create();
        texB.Create();
    }


    public void Load(string filePath)
    {
        byte[] data = File.ReadAllBytes(filePath);
        float[] values = new float[12];
        Buffer.BlockCopy(data, 0, values, 0, values.Length);

        uplift = values[0];
        creepSpeed = values[1];
        rain = values[2];
        bedrockErosionSpeed = values[3];
        regolithErosionSpeed = values[4];
        waterErosionExponent = values[5];
        waterErosionExponent2 = values[6];
        sedimentationRate = values[7];
        weatheringRate = values[8];
        regolithProtectionExponent = values[9];
        regolithProtectionExponent2 = values[10];
        timestepLength = values[11];
    }

    public void Save(string filePath) 
    {
        byte[] data = new byte[12 * sizeof(float)];
        Buffer.BlockCopy(simulationParameters,0,data,0, data.Length);
        File.WriteAllBytes(filePath, data);
    }
}