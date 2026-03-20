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
	public partial class StreamForm : Form
	{
		Channel _channel;
		private Stream CurrentlySelectedStream = null;
		private bool SelectionActive = false;

		public StreamForm(Channel channel)
		{
			_channel = channel;
			InitializeComponent();
		}

		private void StreamForm_Load(object sender, EventArgs e)
		{
			try
			{
				SelectionActive = false;

				if (_channel != null)
				{
					foreach (var strm in _channel)
					{
						Object[] arr = new Object[3];

						arr[0] = strm.name;

						Structure strct = strm.structure;

						arr[1] = strct.name;
						arr[2] = strm.elementCount();

						dataGridView1.Rows.Add(arr);
					}
				}

				dataGridView1.ClearSelection();
				CurrentlySelectedStream = null;

				SelectionActive = true;
			}

			catch
			{
				Console.WriteLine("Some weird error occured");
			}
		}

		private void dataGridView1_SelectionChanged(object sender, EventArgs e)
		{
			if (SelectionActive && dataGridView1.SelectedRows.Count > 0)
			{
				if (dataGridView1[0, dataGridView1.SelectedRows[0].Index].Value != null)
				{
					int Steps = dataGridView1.SelectedRows[0].Index;
					foreach (Stream strm in _channel)
					{
						if (Steps == 0)
						{
							CurrentlySelectedStream = strm;
							break;
						}

						Steps--;
					}
				}
			}
		}

		private void StreamForm_FormClosing(object sender, FormClosingEventArgs e)
		{
			_channel = null;
			CurrentlySelectedStream = null;
		}

		private void dataGridView1_CellMouseDoubleClick(object sender, DataGridViewCellMouseEventArgs e)
		{
			// Examine the currently selected row
			if (CurrentlySelectedStream != null)
			{
				if (CurrentlySelectedStream.Count > 0)
				{
					HandleForm w = new HandleForm(CurrentlySelectedStream);
					w.Text = "Handle(s) available on stream " + CurrentlySelectedStream.name;
					w.ShowDialog();
				}
			}
		}
	}
}
