using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;

using Autodesk.Maya.OpenMaya;
using Autodesk.Maya.MetaData;

namespace metadata
{
	// Information about this column data (access info within the handle)
	struct ColumnStructure
	{
		public uint memberindex;
		public string Header;
		public string Name;
		public string type;
		public uint length;
		public uint arrayindex;
		public uint colindex;
	}

	public partial class HandleForm : Form
	{
		Stream _strm;

		List<ColumnStructure> RowDef;
		List<Handle> RowList;

		ColumnStructure CurrentColumnDef;
		Handle CurrentCellHandle;

		int StartHandleColumn = 0;


		public HandleForm(Stream strm)
		{
			_strm = strm;

			RowDef = new List<ColumnStructure>();
			RowList = new List<Handle>();

			InitializeComponent();
		}

		private void HandleForm_Load(object sender, EventArgs e)
		{
			try
			{
				if (_strm != null)
				{
					RowDef.Clear();
					uint currentcol = 0;

					// Get the structure of the metadata
					Structure strct = _strm.structure;
					uint MemberIndex = 0;
					foreach (Member member in strct)
					{
						for (uint i = 0; i < member.lengthProperty; i++)
						{
							var cs = new ColumnStructure();
							string Header = member.nameProperty;
							string Name = member.nameProperty;

							if (member.lengthProperty > 1)
							{
								Header += "[" + i.ToString() + "]";
								Name += i.ToString();
							}

							cs.Header = Header;
							cs.Name = Name;
							cs.type = member.typeProperty.ToString();
							cs.length = member.lengthProperty;
							cs.memberindex = MemberIndex;
							cs.arrayindex = i;
							cs.colindex = currentcol;

							RowDef.Add(cs);
							currentcol++;
						}

						MemberIndex++;
					}

					dataGridView1.Columns.Clear();

					// Init the static columns
					dataGridView1.Columns.Add("Key", "Key");
					StartHandleColumn++;

					foreach (var cell in RowDef)
						dataGridView1.Columns.Add(cell.Name, cell.Header);

					int HandleNo = 0;
					foreach (var kvp in _strm as IEnumerable<KeyValuePair<Autodesk.Maya.MetaData.Index, Handle>>)
					{
						Object[] arr = new Object[RowDef.Count + StartHandleColumn];

						arr[0] = kvp.Key.asString;

						int colno = StartHandleColumn;
						foreach (var cell in RowDef)
						{
							kvp.Value.setPositionByMemberIndex(cell.memberindex);

							Array data = kvp.Value.asType;
							if (data.Length < 1)
								throw new ApplicationException("Handle data seems corrupted");

							arr[colno] = data.GetValue(cell.arrayindex).ToString();

							colno++;
						}

						HandleNo++;
						RowList.Add(kvp.Value);

						dataGridView1.Rows.Add(arr);
					}

					foreach (DataGridViewRow row in dataGridView1.Rows)
						dataGridView1.Rows[row.Index].HeaderCell.Value = (row.Index.ToString());

					dataGridView1.RowHeadersWidth = 60;
				}

				dataGridView1.ClearSelection();

				dataGridView1.Columns[0].ReadOnly = true;
				dataGridView1.Columns[0].DefaultCellStyle.BackColor = SystemColors.Control;
				dataGridView1.Columns[0].Frozen = true;
				dataGridView1.Columns[0].Width = 50;
			}

			catch
			{
				Console.WriteLine("Some weird error occured");
			}
		}

		private void dataGridView1_CellValidating(object sender, DataGridViewCellValidatingEventArgs e)
		{
			CurrentCellHandle = null;
			bool convertworked = true;
			string value = e.FormattedValue.ToString();

			if (e.ColumnIndex < StartHandleColumn)
			{
				dataGridView1.ClearSelection();
				return;
			}
			else
			{
				// Convert the string cell to the right type
				switch (RowDef[e.ColumnIndex - StartHandleColumn].type)
				{
					case "kBoolean":
						try
						{
							bool v = Convert.ToBoolean(value);
						}

						catch
						{
							convertworked = false;
						}
						break;
					case "kDouble":
						try
						{
							Double v = Convert.ToDouble(value);
						}

						catch
						{
							convertworked = false;
						}
						break;
					case "kFloat":
						try
						{
							Single v = Convert.ToSingle(value);
						}

						catch
						{
							convertworked = false;
						}
						break;
					case "kInt8":
						try
						{
							SByte v = Convert.ToSByte(value);
						}

						catch
						{
							convertworked = false;
						}
						break;
					case "kInt16":
						try
						{
							Int16 v = Convert.ToInt16(value);
						}

						catch
						{
							convertworked = false;
						}
						break;
					case "kInt32":
						try
						{
							Int32 v = Convert.ToInt32(value);
						}

						catch
						{
							convertworked = false;
						}
						break;
					case "kInt64":
						try
						{
							Int64 v = Convert.ToInt64(value);
						}

						catch
						{
							convertworked = false;
						}
						break;
					case "kUInt8":
						try
						{
							Byte v = Convert.ToByte(value);
						}

						catch
						{
							convertworked = false;
						}
						break;
					case "kUInt16":
						try
						{
							UInt16 v = Convert.ToUInt16(value);
						}

						catch
						{
							convertworked = false;
						}
						break;
					case "kUInt32":
						try
						{
							UInt32 v = Convert.ToUInt32(value);
						}

						catch
						{
							convertworked = false;
						}
						break;
					case "kUInt64":
						try
						{
							UInt64 v = Convert.ToUInt64(value);
						}

						catch
						{
							convertworked = false;
						}
						break;
				}

				if (convertworked)
				{
					CurrentCellHandle = RowList[e.RowIndex];
					CurrentColumnDef = RowDef[e.ColumnIndex - StartHandleColumn];
				}
			}

			e.Cancel = !convertworked;
		}

		private void dataGridView1_CellValidated(object sender, DataGridViewCellEventArgs e)
		{
			if (dataGridView1.CurrentCell.IsInEditMode && CurrentCellHandle != null)
			{
				// Set the proper target handle
				CurrentCellHandle.setPositionByMemberIndex(CurrentColumnDef.memberindex);

				switch (CurrentColumnDef.type)
				{
					case "kBoolean":
						{
							bool[] data = new bool[CurrentColumnDef.length];

							// Copy the entire array
							for(int i = 0; i < CurrentColumnDef.length; i++)
								data[i] = Convert.ToBoolean(dataGridView1[(int)(StartHandleColumn + CurrentColumnDef.colindex - CurrentColumnDef.arrayindex + i), e.RowIndex].Value.ToString());
						
							CurrentCellHandle.asBooleanArray = data;
						}
						break;
					case "kString":
						{
							string[] data = new string[CurrentColumnDef.length];

							// Copy the entire array
							for(int i = 0; i < CurrentColumnDef.length; i++)
								data[i] = dataGridView1[(int)(StartHandleColumn + CurrentColumnDef.colindex - CurrentColumnDef.arrayindex + i), e.RowIndex].Value.ToString();
						
							CurrentCellHandle.asStringArray = data;
						}
						break;
					case "kDouble":
						{
							double[] data = new double[CurrentColumnDef.length];

							// Copy the entire array
							for(int i = 0; i < CurrentColumnDef.length; i++)
								data[i] = Convert.ToDouble(dataGridView1[(int)(StartHandleColumn + CurrentColumnDef.colindex - CurrentColumnDef.arrayindex + i), e.RowIndex].Value.ToString());
						
							CurrentCellHandle.asDoubleArray = data;
						}
						break;
					case "kFloat":
						{
							Single[] data = new Single[CurrentColumnDef.length];

							// Copy the entire array
							for (int i = 0; i < CurrentColumnDef.length; i++)
								data[i] = Convert.ToSingle(dataGridView1[(int)(StartHandleColumn + CurrentColumnDef.colindex - CurrentColumnDef.arrayindex + i), e.RowIndex].Value.ToString());

							CurrentCellHandle.asFloatArray = data;
						}
						break;
					case "kInt8":
						{
							SByte[] data = new SByte[CurrentColumnDef.length];

							// Copy the entire array
							for (int i = 0; i < CurrentColumnDef.length; i++)
								data[i] = Convert.ToSByte(dataGridView1[(int)(StartHandleColumn + CurrentColumnDef.colindex - CurrentColumnDef.arrayindex + i), e.RowIndex].Value.ToString());

							CurrentCellHandle.asInt8Array = data;
						}
						break;
					case "kInt16":
						{
							Int16[] data = new Int16[CurrentColumnDef.length];

							// Copy the entire array
							for (int i = 0; i < CurrentColumnDef.length; i++)
								data[i] = Convert.ToInt16(dataGridView1[(int)(StartHandleColumn + CurrentColumnDef.colindex - CurrentColumnDef.arrayindex + i), e.RowIndex].Value.ToString());

							CurrentCellHandle.asInt16Array = data;
						}
						break;
					case "kInt32":
						{
							Int32[] data = new Int32[CurrentColumnDef.length];

							// Copy the entire array
							for (int i = 0; i < CurrentColumnDef.length; i++)
								data[i] = Convert.ToInt32(dataGridView1[(int)(StartHandleColumn + CurrentColumnDef.colindex - CurrentColumnDef.arrayindex + i), e.RowIndex].Value.ToString());

							CurrentCellHandle.asInt32Array = data;
						}
						break;
					case "kInt64":
						{
							Int64[] data = new Int64[CurrentColumnDef.length];

							// Copy the entire array
							for (int i = 0; i < CurrentColumnDef.length; i++)
								data[i] = Convert.ToInt64(dataGridView1[(int)(StartHandleColumn + CurrentColumnDef.colindex - CurrentColumnDef.arrayindex + i), e.RowIndex].Value.ToString());

							CurrentCellHandle.asInt64Array = data;
						}
						break;
					case "kUInt8":
						{
							Byte[] data = new Byte[CurrentColumnDef.length];

							// Copy the entire array
							for (int i = 0; i < CurrentColumnDef.length; i++)
								data[i] = Convert.ToByte(dataGridView1[(int)(StartHandleColumn + CurrentColumnDef.colindex - CurrentColumnDef.arrayindex + i), e.RowIndex].Value.ToString());

							CurrentCellHandle.asUInt8Array = data;
						}
						break;
					case "kUInt16":
						{
							UInt16[] data = new UInt16[CurrentColumnDef.length];

							// Copy the entire array
							for (int i = 0; i < CurrentColumnDef.length; i++)
								data[i] = Convert.ToUInt16(dataGridView1[(int)(StartHandleColumn + CurrentColumnDef.colindex - CurrentColumnDef.arrayindex + i), e.RowIndex].Value.ToString());

							CurrentCellHandle.asUInt16Array = data;
						}
						break;
					case "kUInt32":
						{
							UInt32[] data = new UInt32[CurrentColumnDef.length];

							// Copy the entire array
							for (int i = 0; i < CurrentColumnDef.length; i++)
								data[i] = Convert.ToUInt32(dataGridView1[(int)(StartHandleColumn + CurrentColumnDef.colindex - CurrentColumnDef.arrayindex + i), e.RowIndex].Value.ToString());

							CurrentCellHandle.asUInt32Array = data;
						}
						break;
					case "kUInt64":
						{
							UInt64[] data = new UInt64[CurrentColumnDef.length];

							// Copy the entire array
							for (int i = 0; i < CurrentColumnDef.length; i++)
								data[i] = Convert.ToUInt64(dataGridView1[(int)(StartHandleColumn + CurrentColumnDef.colindex - CurrentColumnDef.arrayindex + i), e.RowIndex].Value.ToString());

							CurrentCellHandle.asUInt64Array = data;
						}
						break;
				}
			}
		}

		private void HandleForm_FormClosing(object sender, FormClosingEventArgs e)
		{
			_strm = null;

			RowDef.Clear();
			RowList.Clear();

			CurrentCellHandle = null;
		}
	}
}
