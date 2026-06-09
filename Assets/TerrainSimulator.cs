using UnityEngine;

public class TerrainSimulator
{
    ComputeShader simulator;

    bool readA = true;
    RenderTextureDescriptor texDescriptor;
    RenderTexture texA = null;
    RenderTexture texB = null;

    public Texture states { get { return readA ? texA : texB; } }

    int generationKernel = -1;
    int simulationKernel = -1;
     
    private float[] simulationParameters = new float[8];
    public float creepSpeed { get { return simulationParameters[0]; } set { simulationParameters[0] = value; simulator.SetFloat("c", simulationParameters[0]); } }
    public float erosionSpeed { get { return simulationParameters[1]; } set { simulationParameters[1] = value; simulator.SetFloat("e", simulationParameters[1]); } }
    public float streamPowerExponent { get { return simulationParameters[2]; } set { simulationParameters[2] = value; simulator.SetFloat("m", simulationParameters[2]); } } // water depth multiplier
    public float streamPowerExponent2 { get { return simulationParameters[3]; } set { simulationParameters[3] = value; simulator.SetFloat("n", simulationParameters[3]); } } // slope multiplier
    public float streamPowerExponent3 { get { return simulationParameters[4]; } set { simulationParameters[4] = value; simulator.SetFloat("d", simulationParameters[4]); } } // water depth decay (Peak erosion at depth of m * d)
    public float sedimentationRate { get { return simulationParameters[5]; } set { simulationParameters[5] = value; simulator.SetFloat("sed", simulationParameters[5]); } }
    public float rainRate { get { return simulationParameters[6]; } set { simulationParameters[6] = value; simulator.SetFloat("r", simulationParameters[6]); } } // Value between 0-1
    public float timestepLength { get { return simulationParameters[7]; } set { simulationParameters[7] = value; simulator.SetFloat("dt", simulationParameters[7]); } }

    public TerrainSimulator(ComputeShader simulator)
    {
        this.simulator = simulator;

        texDescriptor = new RenderTextureDescriptor(1,1,RenderTextureFormat.RFloat, 0, 0, RenderTextureReadWrite.Linear);
        texDescriptor.dimension = UnityEngine.Rendering.TextureDimension.Tex2DArray;
        texDescriptor.enableRandomWrite = true;
        
        timestepLength = 0.1f;
        creepSpeed = 0.001f;
        erosionSpeed = 0.1f;
        streamPowerExponent = 0.4f;
        streamPowerExponent2 = 0.8f;
        streamPowerExponent3 = 1000;
        sedimentationRate = 5f;
        rainRate = 0.4f;
        simulator.SetFloats("windSpeed", new float[] { 3000, 0 });
    }

    public void OnDestroy()
    {
        if(texA) texA.Release();
        if(texB) texB.Release();
    }

    public void GenerateSeedTexture(uint octaves, float cellSize, Vector4 seed)
    {
        simulator.SetFloat("cellSize", cellSize);
        simulator.SetFloat("doubleCellSize", cellSize * 2);
        simulator.SetFloat("cellSizeSquared", cellSize * cellSize);

        simulator.SetInt("octaves", (int)octaves);
        simulator.SetFloat("startFrequency", cellSize / (1.220703125f * Mathf.Max(texA.width, texA.height)));

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
        if (simulationKernel == -1)
        {
            simulationKernel = simulator.FindKernel("Simulate");
        }

        simulator.SetTexture(simulationKernel, "input", readA ? texA : texB);
        simulator.SetTexture(simulationKernel, "result", readA ? texB : texA);

        simulator.Dispatch(simulationKernel, threadGroups.x, threadGroups.y, threadGroups.z);

        readA = !readA;
    }


    public void Init(Vector2Int gridDimensions, int stateVariables, int airLayers)
    {
        texDescriptor.width = gridDimensions.x;
        texDescriptor.height = gridDimensions.y;
        texDescriptor.volumeDepth = stateVariables;

        int[] dim = { gridDimensions.x, gridDimensions.y };
        simulator.SetInts("dim", dim);
        simulator.SetInt("airLayers", airLayers);

        float left = -((float)gridDimensions.x) / 2;
        float bottom = -((float)gridDimensions.y) / 2;

        simulator.SetFloat("width", gridDimensions.x);
        simulator.SetFloat("height", gridDimensions.y);
        simulator.SetFloat("left", left);
        simulator.SetFloat("bottom", bottom);

        if (texA != null)
        {
            texA.Release();
            texA.width = gridDimensions.x;
            texA.height = gridDimensions.y;
        }
        else
        {
            texA = new RenderTexture(texDescriptor);
        }
        if (texB != null)
        {
            texB.Release();
            texB.width = gridDimensions.x;
            texB.height = gridDimensions.y;
        }
        else
        {
            texB = new RenderTexture(texDescriptor);
        }

        texA.Create();
        texB.Create();
    }
}