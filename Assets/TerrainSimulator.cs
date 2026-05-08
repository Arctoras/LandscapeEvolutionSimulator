using System;
using UnityEngine;
using UnityEngine.Windows;

public class TerrainSimulator
{
    ComputeShader simulator;

    bool readA = true;
    RenderTexture texA = null;
    RenderTexture texB = null;
    RenderTexture texInter = null;

    public Texture states { get { return readA ? texA : texB; } }

    int generationKernel = -1;
    int simulationKernel = -1;
    int simInterKernel = -1;

    private float[] simulationParameters = new float[14];
    public float uplift { get { return simulationParameters[0]; } set { simulationParameters[0] = value; simulator.SetFloat("u", simulationParameters[0]); } }
    public float creepSpeed { get { return simulationParameters[1]; } set { simulationParameters[1] = value; simulator.SetFloat("c", simulationParameters[1]); } }
    public float rain { get { return simulationParameters[2]; } set { simulationParameters[2] = value; simulator.SetFloat("r", simulationParameters[2]); } } // amount of rain per pixel
    public float bedrockErosionSpeed { get { return simulationParameters[3]; } set { simulationParameters[3] = value; simulator.SetFloat("e0", simulationParameters[3]); } }
    public float regolithErosionSpeed { get { return simulationParameters[4]; } set { simulationParameters[4] = value; simulator.SetFloat("e1", simulationParameters[4]); } }
    public float waterErosionExponent { get { return simulationParameters[5]; } set { simulationParameters[5] = value; simulator.SetFloat("m", simulationParameters[5]); } } // water depth multiplier
    public float waterErosionExponent2 { get { return simulationParameters[6]; } set { simulationParameters[6] = value; simulator.SetFloat("n", simulationParameters[6]); } } // slope multiplier
    public float waterTransportExponent { get { return simulationParameters[7]; } set { simulationParameters[7] = value; simulator.SetFloat("a", simulationParameters[7]); } } // water depth multiplier
    public float waterTransportExponent2 { get { return simulationParameters[8]; } set { simulationParameters[8] = value; simulator.SetFloat("b", simulationParameters[8]); } } // slope multiplier
    public float sedimentationRate { get { return simulationParameters[9]; } set { simulationParameters[9] = value; simulator.SetFloat("s", simulationParameters[9]); } }
    public float weatheringRate { get { return simulationParameters[10]; } set { simulationParameters[10] = value; simulator.SetFloat("w", simulationParameters[10]); } }
    public float regolithProtectionExponent { get { return simulationParameters[11]; } set { simulationParameters[11] = value; simulator.SetFloat("p0", simulationParameters[11]); } } // protecting against bedrock erosion (Cannot be 0)
    public float regolithProtectionExponent2 { get { return simulationParameters[12]; } set { simulationParameters[12] = value; simulator.SetFloat("p1", simulationParameters[12]); } } // protecting against bedrock weathering (Cannot be 0)  
    public float timestepLength { get { return simulationParameters[13]; } set { simulationParameters[13] = value; simulator.SetFloat("dt", simulationParameters[13]); } } // timestep

    public TerrainSimulator(ComputeShader simulator)
    {
        this.simulator = simulator;

        if (File.Exists("config/lastSimParams.data")) Load("config/lastSimParams.data");
        else if (File.Exists("config/defaultSimParams.data")) Load("config/defaultSimParams.data");

        timestepLength = 0.01f;
    }

    public void OnDestroy()
    {
        if(texA) texA.Release();
        if(texB) texB.Release();
        if(texInter) texInter.Release();

        if (!Directory.Exists("config")) Directory.CreateDirectory("config");
        Save("config/lastSimParams.data");
    }

    public void GenerateSeedTexture(uint octaves, float cellSize, Vector4 seed)
    {
        int[] dim = { texA.width, texA.height };
        simulator.SetInts("dim", dim);
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
    }

    public void RunSimulationStep(Vector3Int threadGroups)
    {
        if (simInterKernel == -1)
        {
            simInterKernel = simulator.FindKernel("Inter");
            simulator.SetTexture(simInterKernel, "interResult", texInter);
        }

        if (simulationKernel == -1)
        {
            simulationKernel = simulator.FindKernel("Simulate");
            simulator.SetTexture(simulationKernel, "interInput", texInter);
        }

        simulator.SetTexture(simInterKernel, "input", readA ? texA : texB);
        simulator.SetTexture(simulationKernel, "input", readA ? texA : texB);
        simulator.SetTexture(simulationKernel, "result", readA ? texB : texA);

        simulator.Dispatch(simInterKernel, threadGroups.x, threadGroups.y, threadGroups.z);
        simulator.Dispatch(simulationKernel, threadGroups.x, threadGroups.y, threadGroups.z);

        readA = !readA;
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
        if (texInter != null)
        {
            texInter.Release();
            texInter.width = gridDimensions.x;
            texInter.height = gridDimensions.y;
        }
        else
        {
            texInter = new RenderTexture(gridDimensions.x, gridDimensions.y, 0, RenderTextureFormat.ARGBFloat, RenderTextureReadWrite.Linear);
            texInter.enableRandomWrite = true;
        }

        texA.Create();
        texB.Create();
        texInter.Create();
    }


    public void Load(string filePath)
    {
        byte[] data = File.ReadAllBytes(filePath);
        float[] values = new float[14];
        Buffer.BlockCopy(data, 0, values, 0, values.Length);

        uplift = values[0];
        creepSpeed = values[1];
        rain = values[2];
        bedrockErosionSpeed = values[3];
        regolithErosionSpeed = values[4];
        waterErosionExponent = values[5];
        waterErosionExponent2 = values[6];
        waterTransportExponent = values[7];
        waterTransportExponent2 = values[8];
        sedimentationRate = values[9];
        weatheringRate = values[10];
        regolithProtectionExponent = values[11];
        regolithProtectionExponent2 = values[12];
        timestepLength = values[13];
    }

    public void Save(string filePath) 
    {
        byte[] data = new byte[14 * sizeof(float)];
        Buffer.BlockCopy(simulationParameters,0,data,0, data.Length);
        File.WriteAllBytes(filePath, data);
    }
}